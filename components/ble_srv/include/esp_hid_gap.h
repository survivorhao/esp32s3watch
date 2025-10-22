/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _ESP_HID_GAP_H_
#define _ESP_HID_GAP_H_

#define HIDD_IDLE_MODE 0x00
#define HIDD_BLE_MODE 0x01
#define HIDD_BT_MODE 0x02
#define HIDD_BTDM_MODE 0x03


#if CONFIG_BT_NIMBLE_ENABLED

//nimble stack only have ble mode 
#define HID_DEV_MODE HIDD_BLE_MODE

#else
#error "if want to use ble hid device please select nimble stack"

#define HID_DEV_MODE HIDD_IDLE_MODE

#endif

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_hid_common.h"


#include "custom_event.h"


#ifdef __cplusplus
extern "C" {
#endif


esp_err_t esp_hid_gap_init(uint8_t mode);

void esp_hid_gap_deinit(void);

void ble_init_controller(void);



esp_err_t esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name);
esp_err_t esp_hid_ble_gap_adv_start(void);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_HIDH_GAP_H_ */
