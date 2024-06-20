#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#define GATTS_TAG "CANDY_ABCD"
#define BLE_DEVICE_NAME "CANDY_ABCD"

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a9"

// 서비스 UUID: 4B9131C3-C9C5-CC8F-9E45-B51F01C2AF4F
static uint8_t service_uuid[16] = {
    0x4F, 0xAF, 0xC2, 0x01, 0x1F, 0xB5, 0x45, 0x9E, 0x8F, 0xCC, 0xC5, 0xC9, 0xC3, 0x31, 0x4B, 0x91
};

// 특성 UUID RX: A8261B36-07EA-F5B7-8846-E1363E48B5BE
static uint8_t char_uuid_rx[16] = {
    0xBE, 0xB5, 0x48, 0x3E, 0x36, 0xE1, 0x46, 0x88, 0xB7, 0xF5, 0xEA, 0x07, 0x36, 0x1B, 0x26, 0xA8
};

// 특성 UUID TX: beb5483e-36e1-4688-b7f5-ea07361b26a9
static uint8_t char_uuid_tx[16] = {
    0xBE, 0xB5, 0x48, 0x3E, 0x36, 0xE1, 0x46, 0x88, 0xB7, 0xF5, 0xEA, 0x07, 0x36, 0x1B, 0x26, 0xA9
};

// 광고 데이터 설정
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TAG, "Advertising start failed");
            } else {
                ESP_LOGI(GATTS_TAG, "Advertising start successfully");
            }
            break;
        default:
            break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d",
                     param->start.status, param->start.service_handle);
            break;

       case ESP_GATTS_REG_EVT:
            ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);

            esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
            esp_ble_gap_config_adv_data(&adv_data);

            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id.inst_id = 0x00,
                .id.uuid.len = ESP_UUID_LEN_128,
            };
            memcpy(service_id.id.uuid.uuid.uuid128, service_uuid, ESP_UUID_LEN_128);
            esp_ble_gatts_create_service(gatts_if, &service_id, 8);
            break;


         case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
            esp_ble_gatts_start_service(param->create.service_handle);

            esp_bt_uuid_t tx_char_uuid = {
                .len = ESP_UUID_LEN_128,
            };
            memcpy(tx_char_uuid.uuid.uuid128, char_uuid_tx, ESP_UUID_LEN_128);

            esp_bt_uuid_t rx_char_uuid = {
                .len = ESP_UUID_LEN_128,
            };
            memcpy(rx_char_uuid.uuid.uuid128, char_uuid_rx, ESP_UUID_LEN_128);

            esp_gatt_char_prop_t char_property = ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_WRITE;

            // RX 특성 추가
            esp_err_t add_char_ret = esp_ble_gatts_add_char(param->create.service_handle, &rx_char_uuid,
                                                            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                            char_property, NULL, NULL);
            if (add_char_ret) {
                ESP_LOGE(GATTS_TAG, "add RX char failed, error code =%x", add_char_ret);
            } else {
                ESP_LOGI(GATTS_TAG, "add RX char successful");
            }

            // TX 특성 추가
            add_char_ret = esp_ble_gatts_add_char(param->create.service_handle, &tx_char_uuid,
                                                  ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                  char_property, NULL, NULL);
            if (add_char_ret) {
                ESP_LOGE(GATTS_TAG, "add TX char failed, error code =%x", add_char_ret);
            } else {
                ESP_LOGI(GATTS_TAG, "add TX char successful");
            }
            break;

         case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(GATTS_TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d",
                     param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
            // 디스크립터 추가
            esp_bt_uuid_t descr_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
            };
            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(param->add_char.service_handle, &descr_uuid,
                                                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
            if (add_descr_ret) {
                ESP_LOGE(GATTS_TAG, "add char descr failed, error code =%x", add_descr_ret);
            } else {
                ESP_LOGI(GATTS_TAG, "add char descr successful");
            }
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d", param->connect.conn_id);
            ESP_LOGI(GATTS_TAG, "Connected device address: %02x:%02x:%02x:%02x:%02x:%02x",
                     param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                     param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
            ESP_LOGI(GATTS_TAG, "Service UUID: %s", SERVICE_UUID);
            ESP_LOGI(GATTS_TAG, "Characteristic UUID RX: %s", CHARACTERISTIC_UUID_RX);
            ESP_LOGI(GATTS_TAG, "Characteristic UUID TX: %s", CHARACTERISTIC_UUID_TX);
            break;
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %" PRIu16, 
                     param->write.conn_id, param->write.trans_id, param->write.handle);
            ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
            esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
            
            // Send response if needed
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;

        case ESP_GATTS_READ_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %" PRIu16, 
                     param->read.conn_id, param->read.trans_id, param->read.handle);
            // Prepare a response
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 4;  // Example length
            rsp.attr_value.value[0] = 0xde;  // Example data
            rsp.attr_value.value[1] = 0xad;
            rsp.attr_value.value[2] = 0xbe;
            rsp.attr_value.value[3] = 0xef;
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "ESP_GATTS_DISCONNECT_EVT, reason %d", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;

        default:
            break;
    }
}

void app_main(void) {
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BLE
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Register GAP callback
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));

    // Register GATT callback
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_profile_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));

    // Set advertisement data
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
}
