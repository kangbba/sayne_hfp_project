/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "bt_app_core.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "bt_app_hf.h"
#include "gpio_pcm_config.h"
#include "esp_console.h"
#include "app_hf_msg_set.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"

#define DEVICE_NAME "CANDY5"
#define GATTS_TAG "GATTS_DEMO"

// UUID 정의
static esp_gatt_srvc_id_t service_uuid = {
    .id = {
        .uuid = {
            .len = ESP_UUID_LEN_128,
            .uuid.uuid128 = {0x4f, 0xaf, 0xc2, 0x01, 0x1f, 0xb5, 0x45, 0x9e, 0x8f, 0xcc, 0xc5, 0xc9, 0xc3, 0x31, 0x91, 0x4b},
        },
        .inst_id = 0,
    },
    .is_primary = true,
};

static esp_bt_uuid_t char_uuid_rx = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {0xbe, 0xb5, 0x48, 0x3e, 0x36, 0xe1, 0x46, 0x88, 0xb7, 0xf5, 0xea, 0x07, 0x36, 0x1b, 0x26, 0xa8}},
};

static esp_bt_uuid_t char_uuid_tx = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {0xbe, 0xb5, 0x48, 0x3e, 0x36, 0xe1, 0x46, 0x88, 0xb7, 0xf5, 0xea, 0x07, 0x36, 0x1b, 0x26, 0xa9}},
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = service_uuid.id.uuid.uuid.uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// 전역 변수 선언
static const char remote_device_name[] = DEVICE_NAME;
esp_bd_addr_t peer_addr = {0};
static char peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
static uint8_t peer_bdname_len;

static bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        for (int i = 0; i < param->disc_res.num_prop; i++) {
            if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR
                && get_name_from_eir(param->disc_res.prop[i].val, peer_bdname, &peer_bdname_len)) {
                if (strcmp(peer_bdname, remote_device_name) == 0) {
                    memcpy(peer_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
                    ESP_LOGI(BT_HF_TAG, "Found a target device address:");
                    ESP_LOG_BUFFER_HEX(BT_HF_TAG, peer_addr, ESP_BD_ADDR_LEN);
                    ESP_LOGI(BT_HF_TAG, "Found a target device name: %s", peer_bdname);
                    printf("Connect.\n");
                    esp_hf_client_connect(peer_addr);
                    esp_bt_gap_cancel_discovery();
                }
            }
        }
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_DISC_STATE_CHANGED_EVT");
    case ESP_BT_GAP_RMT_SRVCS_EVT:
    case ESP_BT_GAP_RMT_SRVC_REC_EVT:
        break;
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_HF_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            ESP_LOG_BUFFER_HEX(BT_HF_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        } else {
            ESP_LOGE(BT_HF_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT: {
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit) {
            ESP_LOGI(BT_HF_TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
            ESP_LOGI(BT_HF_TAG, "Input pin code: 1234");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %"PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%"PRIu32, param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d", param->mode_chg.mode);
        break;

    default: {
        ESP_LOGI(BT_HF_TAG, "event: %d", event);
        break;
    }
    }
    return;
}

/* event for handler "bt_av_hdl_stack_up */
enum {
    BT_APP_EVT_STACK_UP = 0,
};

/* handler for bluetooth stack enabled events */
static void bt_hf_client_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(BT_HF_TAG, "%s evt %d", __func__, event);
    switch (event) {
    case BT_APP_EVT_STACK_UP: {
        esp_bt_gap_set_device_name(DEVICE_NAME);
        esp_bt_gap_register_callback(esp_bt_gap_cb);
        esp_hf_client_register_callback(bt_app_hf_client_cb);
        esp_hf_client_init();
        esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
        esp_bt_pin_code_t pin_code;
        pin_code[0] = '0';
        pin_code[1] = '0';
        pin_code[2] = '0';
        pin_code[3] = '0';
        esp_bt_gap_set_pin(pin_type, 4, pin_code);
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
        break;
    }
    default:
        ESP_LOGE(BT_HF_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}



// BLE 관련 코드
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
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

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        esp_ble_gap_set_device_name(DEVICE_NAME);
        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gatts_create_service(gatts_if, &service_uuid, 4);
        break;
    case ESP_GATTS_CREATE_EVT: {
        esp_gatt_char_prop_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
        esp_gatt_char_prop_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
        esp_ble_gatts_add_char(param->create.service_handle, &char_uuid_rx, ESP_GATT_PERM_WRITE, char_prop_write, NULL, NULL);
        esp_ble_gatts_add_char(param->create.service_handle, &char_uuid_tx, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, char_prop_read_notify, NULL, NULL);
        break;
    }
    default:
        break;
    }
}void init_ble(void)
{
    esp_err_t ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(GATTS_TAG, "init_ble enable controller failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(GATTS_TAG, "init_ble initialize bluedroid failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(GATTS_TAG, "init_ble enable bluedroid failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatt_set_local_mtu(500);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", ret);
    }
}

void init_hfp(void)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(BT_HF_TAG, "init_hfp initialize controller failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "init_hfp enable controller failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(BT_HF_TAG, "init_hfp initialize bluedroid failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(BT_HF_TAG, "init_hfp enable bluedroid failed: %s", esp_err_to_name(ret));
        return;
    }

    bt_app_task_start_up();
    bt_app_work_dispatch(bt_hf_client_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_hfp();
    init_ble();

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    repl_config.prompt = "hfp_hf>";

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    register_hfp_hf();
    printf("\n ==================================================\n");
    printf(" |       Steps to test hfp_hf                     |\n");
    printf(" |                                                |\n");
    printf(" |  1. Print 'help' to gain overview of commands  |\n");
    printf(" |  2. Setup a service level connection           |\n");
    printf(" |  3. Run hfp_hf to test                         |\n");
    printf(" |                                                |\n");
    printf(" =================================================\n\n");

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

#define PROFILE_A_APP_ID 0

static esp_gatt_char_prop_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static esp_gatt_char_prop_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static esp_attr_value_t gatts_demo_char1_val = {
    .attr_max_len = 0x40,
    .attr_len = sizeof(uint8_t),
    .attr_value = NULL,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    // gap_event_handler implementation
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    // gatts_event_handler implementation
}
