#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"

// 로그 태그 정의
#define GATTS_TAG "GATTS_DEMO"

// 서비스 및 특성 UUID 정의
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// 디바이스 이름 정의
#define DEVICE_NAME "TamiOn_3708"

// 서비스 핸들 정의
static uint16_t service_handle;
static uint16_t char_handle;

// 128비트 UUID 변환 함수
static void convert_str_to_uuid128(const char* str, esp_bt_uuid_t* uuid) {
    uint8_t *p = uuid->uuid.uuid128;
    int i, j;
    for (i = 0, j = 0; i < 16; ++i, j += 2) {
        p[i] = (str[j] >= 'a' ? (str[j] - 'a' + 10) : (str[j] - '0')) << 4;
        p[i] |= (str[j + 1] >= 'a' ? (str[j + 1] - 'a' + 10) : (str[j + 1] - '0'));
    }
}

// 광고 데이터 설정
static uint8_t adv_service_uuid128[16] = {
    0x4b, 0x91, 0x31, 0xc3, 0xc5, 0xc9, 0xcc, 0x8f, 
    0x9e, 0x45, 0xb5, 0x1f, 0x01, 0xc2, 0xaf
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,            // 스캔 응답 패킷 설정 (false: 광고 패킷 사용)
    .include_name = true,             // 디바이스 이름 포함 여부 (true)
    .include_txpower = false,         // TX 전력 포함 여부 (false)
    .min_interval = 0x0006,           // 광고 최소 간격
    .max_interval = 0x0010,           // 광고 최대 간격
    .appearance = 0x00,               // 외형 설정 (0x00: 기본 값)
    .manufacturer_len = 0,            // 제조사 데이터 길이 (0)
    .p_manufacturer_data = NULL,      // 제조사 데이터 포인터 (NULL)
    .service_data_len = 0,            // 서비스 데이터 길이 (0)
    .p_service_data = NULL,           // 서비스 데이터 포인터 (NULL)
    .service_uuid_len = 16,           // 서비스 UUID 길이 (16: 128비트 UUID)
    .p_service_uuid = adv_service_uuid128, // 서비스 UUID 포인터
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT) // 광고 플래그 설정
};

// 광고 파라미터 설정
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,             // 광고 최소 간격
    .adv_int_max = 0x40,             // 광고 최대 간격
    .adv_type = ADV_TYPE_IND,        // 광고 타입 (일반 광고)
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC, // 디바이스 주소 타입
    .channel_map = ADV_CHNL_ALL,     // 사용 채널 맵 (모든 채널)
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY, // 광고 필터 정책
};

// GAP 이벤트 핸들러
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    ESP_LOGI(GATTS_TAG, "GAP event handler, event: %d", event);
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        default:
            break;
    }
}

// GATT 서버 이벤트 핸들러
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    static esp_gatt_srvc_id_t service_id;

    ESP_LOGI(GATTS_TAG, "GATT event handler, event: %d", event);

    switch (event) {
        case ESP_GATTS_REG_EVT: {
            // 서비스 ID 설정
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_128;
            convert_str_to_uuid128(SERVICE_UUID, &service_id.id.uuid);

            // 서비스 생성
            esp_ble_gatts_create_service(gatts_if, &service_id, 4);
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(service_handle);

            esp_bt_uuid_t char_uuid;
            char_uuid.len = ESP_UUID_LEN_128;
            convert_str_to_uuid128(CHARACTERISTIC_UUID, &char_uuid);

            esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
            esp_ble_gatts_add_char(service_handle, &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                   property, NULL, NULL);
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            char_handle = param->add_char.attr_handle;
            break;
        }
        case ESP_GATTS_WRITE_EVT: {
            if (param->write.handle == char_handle) {
                ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
                esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);

                // 수신된 데이터를 문자열로 변환
                char recv_data[256];
                snprintf(recv_data, sizeof(recv_data), "%.*s", param->write.len, (char*)param->write.value);
                ESP_LOGI(GATTS_TAG, "Received string: %s", recv_data);

                // 수신된 데이터로 클라이언트에게 알림 (Notify)
                esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, char_handle, param->write.len, param->write.value, false);
            }
            break;
        }
        default:
            break;
    }
}

// 앱 메인 함수
void app_main(void) {
    esp_err_t ret;

    // NVS 초기화
    ESP_LOGI(GATTS_TAG, "Initializing NVS");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(GATTS_TAG, "Erasing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // BT 컨트롤러 초기화
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_LOGI(GATTS_TAG, "Initializing BT controller");
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Initialize controller failed: %s", esp_err_to_name(ret));
        return;
    }

    // BT 컨트롤러 활성화
    ESP_LOGI(GATTS_TAG, "Enabling BT controller");
    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Enable controller failed: %s", esp_err_to_name(ret));
        return;
    }

    // Bluedroid 초기화
    ESP_LOGI(GATTS_TAG, "Initializing Bluedroid");
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Initialize Bluedroid failed: %s", esp_err_to_name(ret));
        return;
    }

    // Bluedroid 활성화
    ESP_LOGI(GATTS_TAG, "Enabling Bluedroid");
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "Enable Bluedroid failed: %s", esp_err_to_name(ret));
        return;
    }

    // GAP 및 GATT 콜백 함수 등록
    ESP_LOGI(GATTS_TAG, "Registering GAP and GATT callbacks");
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);

    // GATT 앱 등록
    ESP_LOGI(GATTS_TAG, "Registering GATT application");
    esp_ble_gatts_app_register(0);

    // 디바이스 이름 설정 및 광고 데이터 설정
    ESP_LOGI(GATTS_TAG, "Setting device name and configuring advertising data");
    ret = esp_ble_gap_set_device_name(DEVICE_NAME);
    if (ret){
        ESP_LOGE(GATTS_TAG, "Set device name failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret){
        ESP_LOGE(GATTS_TAG, "Configure advertising data failed: %s", esp_err_to_name(ret));
        return;
    }
}
