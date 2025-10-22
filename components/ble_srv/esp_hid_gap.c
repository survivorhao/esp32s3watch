/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_random.h"
#include "esp_hid_gap.h"

//use nimble stack (us target is esp32s3)
#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "nimble/ble.h"
#include "host/ble_sm.h"
#endif


#include "custom_event.h"

static const char *TAG = "ESP_HID_GAP";

// uncomment to print all devices that were seen during a scan
#define GAP_DBG_PRINTF(...) //printf(__VA_ARGS__)
//static const char * gap_bt_prop_type_names[5] = {"","BDNAME","COD","RSSI","EIR"};


static SemaphoreHandle_t    bt_hidh_cb_semaphore = NULL;
#define WAIT_BT_CB()        xSemaphoreTake(bt_hidh_cb_semaphore, portMAX_DELAY)
#define SEND_BT_CB()        xSemaphoreGive(bt_hidh_cb_semaphore)

static SemaphoreHandle_t    ble_hidh_cb_semaphore = NULL;
#define WAIT_BLE_CB()       xSemaphoreTake(ble_hidh_cb_semaphore, portMAX_DELAY)
#define SEND_BLE_CB()       xSemaphoreGive(ble_hidh_cb_semaphore)

#define SIZEOF_ARRAY(a) (sizeof(a)/sizeof(*a))


//-------------------------------------------------------------------------------------------------------------------
//--------------------------------------start ---start-----start----------------------------------------------------------------

//----------------The following are the effective functions when using the Nimble stack.

//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------



//use nimble stack
#if CONFIG_BT_NIMBLE_ENABLED
static struct ble_hs_adv_fields fields;
#define GATT_SVR_SVC_HID_UUID 0x1812



/**
 * @brief Initialize BLE advertising fields for HID over GATT using NimBLE.
 *
 * Prepares advertisement fields including flags, tx power, device name and
 * the HID service UUID. Configures security manager parameters for pairing.
 *
 * @param appearance GAP appearance value to advertise.
 * @param device_name Null-terminated device name string to advertise.
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name)
{
    ble_uuid16_t *uuid16, *uuid16_1;
    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (HID).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    uuid16 = (ble_uuid16_t *)malloc(sizeof(ble_uuid16_t));
    uuid16_1 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATT_SVR_SVC_HID_UUID)
    };
    memcpy(uuid16, uuid16_1, sizeof(ble_uuid16_t));
    fields.uuids16 = uuid16;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    /* Initialize the security configuration */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;

    return ESP_OK;

}

/**
 * @brief NimBLE GAP event callback for HID device operations.
 *
 * Handles connection, disconnection, connection parameter updates, advertising
 * completion, subscription changes, MTU updates, encryption change events,
 * notify transmit completion, repeat pairing and passkey actions. Posts
 * relevant application events to the UI event loop when necessary.
 *
 * @param event Pointer to the ble_gap_event supplied by NimBLE.
 * @param arg Opaque user argument (unused).
 * @return 0 on success or NimBLE-specific error code.
 */
static int
nimble_hid_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGW(TAG, "connection %s; status=%d,con_hanle=%d",
                event->connect.status == 0 ? "established" : "failed",
                event->connect.status, event->connect.conn_handle);
        
        //notify to save connection handle
        ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, 
                                APP_BLE_SET_CONN_HANDLE, &(event->connect.conn_handle), sizeof(event->connect.conn_handle), portMAX_DELAY));

    
        return 0;
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "disconnect; reason=%d,con_handle= %d", event->disconnect.reason,
                                                        event->disconnect.conn.conn_handle);

        return 0;
    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        ESP_LOGI(TAG, "connection updated; status=%d",
                event->conn_update.status);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertise complete; reason=%d",
                event->adv_complete.reason);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d "
                "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                event->subscribe.conn_handle,
                event->subscribe.attr_handle,
                event->subscribe.reason,
                event->subscribe.prev_notify,
                event->subscribe.cur_notify,
                event->subscribe.prev_indicate,
                event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                event->mtu.conn_handle,
                event->mtu.channel_id,
                event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
            
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);

        //hid host pass input success
        if(event->enc_change.status==0)
        {
            //post pair success event
            ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_PAIR_SUCCESS, NULL, 0, portMAX_DELAY));
                                    
            //notify to save connection handle
            ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, 
                        APP_BLE_SET_CONN_HANDLE, &(event->enc_change.conn_handle), 
                        sizeof(event->enc_change.conn_handle), portMAX_DELAY));

        }
        
        
        return 0;

    case BLE_GAP_EVENT_NOTIFY_TX:
        MODLOG_DFLT(INFO, "notify_tx event; conn_handle=%d attr_handle=%d "
                "status=%d is_indication=%d",
                event->notify_tx.conn_handle,
                event->notify_tx.attr_handle,
                event->notify_tx.status,
                event->notify_tx.indication);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started");
        struct ble_sm_io pkey = {0};
        int key = 0;

        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            pkey.action = event->passkey.params.action;

            //produce random number
            pkey.passkey = (esp_random()%999999)+1; // This is the passkey to be entered on peer
            
            //post to my_ui task to display key on screen
            ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_BLE_PAIR_PASSKEY_ENTRY, &pkey.passkey, sizeof(pkey.passkey), portMAX_DELAY));
            
            ESP_LOGI(TAG, "Enter passkey %" PRIu32 "on the peer side", pkey.passkey);
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            ESP_LOGI(TAG, "Accepting passkey..");
            pkey.action = event->passkey.params.action;
            pkey.numcmp_accept = key;
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
            static uint8_t tem_oob[16] = {0};
            pkey.action = event->passkey.params.action;
            for (int i = 0; i < 16; i++) {
                pkey.oob[i] = tem_oob[i];
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
            ESP_LOGI(TAG, "Input not supported passing -> 123456");
            pkey.action = event->passkey.params.action;
            pkey.passkey = 123456;
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
        }
        return 0;
    }
    return 0;
}

//use nimble stack
/**
 * @brief Start BLE advertising for HID device using NimBLE.
 *
 * Sets the previously prepared advertisement fields and begins advertising
 * with recommended parameters for HID devices.
 *
 * @return 0 on success or a negative NimBLE error code.
 */
esp_err_t esp_hid_ble_gap_adv_start(void)
{
    int rc;
    struct ble_gap_adv_params adv_params;
    /* maximum possible duration for hid device(180s) */
    int32_t adv_duration_ms = 180000;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return rc;
    }
    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);/* Recommended interval 30ms to 50ms */
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, adv_duration_ms,
                           &adv_params, nimble_hid_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return rc;
    }
    return rc;
}
#endif


//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------

//--------------------------------end end end -----------------------------------------------

//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------



#if CONFIG_BT_NIMBLE_ENABLED
/**
 * @brief Initialize low-level controller for NimBLE-based Bluetooth.
 *
 * Releases Classic BT memory, initializes and enables the controller, and
 * sets up NimBLE stack. Used when CONFIG_BT_NIMBLE_ENABLED is set.
 *
 * @param mode Bitmask specifying controller modes (ESP_BT_MODE_*).
 * @return ESP_OK on success or esp_err_t on failure.
 */
static esp_err_t init_low_level(uint8_t mode)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32
    bt_cfg.mode = mode;
#endif
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %d", ret);
        return ret;
    }
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
        return ret;
    }

    ret = esp_bt_controller_enable(mode);
    if (ret==ESP_OK) 
    {

    }
    else
    {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", ret);
        return ret;
    }

    ret = esp_nimble_init();
    if (ret) {
        ESP_LOGE(TAG, "esp_nimble_init failed: %d", ret);
        return ret;
    }


    return ret;
}

void ble_init_controller(void)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %d", ret);
        return ;
    }
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
        return ;
    }
}

#endif



/**
 * @brief Initialize HID GAP and underlying Bluetooth controller.
 *
 * Validates the provided mode, allocates semaphores used for synchronous
 * callbacks, and calls init_low_level() to initialize the controller and
 * host stack.
 *
 * @param mode Bitmask specifying controller modes (ESP_BT_MODE_*).
 * @return ESP_OK on success or an esp_err_t on failure.
 */
esp_err_t esp_hid_gap_init(uint8_t mode)
{
    esp_err_t ret;
    // Check whether the set mode is within the valid range.
    if (!mode || mode > ESP_BT_MODE_BTDM) 
    {
        ESP_LOGE(TAG, "Invalid mode given!");
        return ESP_FAIL;
    }


    if (bt_hidh_cb_semaphore != NULL) {
        ESP_LOGE(TAG, "Already initialised");
        return ESP_FAIL;
    }

    bt_hidh_cb_semaphore = xSemaphoreCreateBinary();
    if (bt_hidh_cb_semaphore == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
        return ESP_FAIL;
    }

    ble_hidh_cb_semaphore = xSemaphoreCreateBinary();
    if (ble_hidh_cb_semaphore == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
        vSemaphoreDelete(bt_hidh_cb_semaphore);
        bt_hidh_cb_semaphore = NULL;
        return ESP_FAIL;
    }


    // Initialize the Controller layer and  host stack,
    // and register the GAP event callback function.
    ret = init_low_level(mode);
    if (ret != ESP_OK) 
    {
        vSemaphoreDelete(bt_hidh_cb_semaphore);
        bt_hidh_cb_semaphore = NULL;
        vSemaphoreDelete(ble_hidh_cb_semaphore);
        ble_hidh_cb_semaphore = NULL;


        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Deinitialize HID GAP resources.
 *
 * Deletes internal semaphores allocated in esp_hid_gap_init and clears
 * the stored semaphore handles.
 */
void esp_hid_gap_deinit(void)
{
    
    vSemaphoreDelete(bt_hidh_cb_semaphore);
    vSemaphoreDelete(ble_hidh_cb_semaphore);

    bt_hidh_cb_semaphore=NULL;
    ble_hidh_cb_semaphore=NULL;


}




