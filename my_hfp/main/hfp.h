// hfp.h
#ifndef HFP_H
#define HFP_H

#include "esp_bt.h"
#include "esp_gap_bt_api.h" // esp_bt_gap_cb_event_t와 esp_bt_gap_cb_param_t 정의를 포함

// Function prototypes
void init_hfp(void);
void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len);

#endif // HFP_H
