#include <stdio.h>
#include "esp_hidd.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "esp_hidd.h"
#include "esp_hid_gap.h"



#include "ble_srv.h"



//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------
static const char *TAG = "BLE_SRV";

typedef struct {
    TaskHandle_t task_hdl;      // HID device task handle
    esp_hidd_dev_t *hid_dev;    
    uint8_t protocol_mode;  
    uint8_t *buffer;    
    bool connected;             // Connection status flag
    uint16_t conn_handle;       // BLE connection handle
    SemaphoreHandle_t deinit_sem; // Semaphore to wait for disconnect during deinit
    bool is_deinitializing;     // Flag to signal intentional deinit
} local_param_t;


static local_param_t s_ble_hid_param = {0};

const unsigned char mediaReportMap[] = {

    //Top-level Collection (Report ID 3: Media Control) 
    0x05, 0x0C,       // Usage Page (Consumer Devices - 0x0C)
    0x09, 0x01,       // Usage (Consumer Control - 0x01)
    0xA1, 0x01,       // Collection (Application)

    0x85, 0x03,       // Report ID (3)  <-- Using Report ID 3

    //Define input fields (bit fields) for 5 media keys.
    0x15, 0x00,       // Logical Minimum (0)  <-- 0 indicates not pressed
    0x25, 0x01,       // Logical Maximum (1)  <-- 1 indicates pressed

    // Use the Usage Minimum/Maximum range to define 5 buttons at once.
    // Ensure that the sequence of these 5 Usage IDs you define is continuous, or define them through multiple Usage Items.
    
    //define the 5 keys you need (Bit 0 - Bit 4)
    0x09, 0xB5,       //   Usage (Scan Next Track)
    0x09, 0xB6,       //   Usage (Scan Previous Track)
    0x09, 0xCD,       //   Usage (Play/Pause)
    0x09, 0xE9,       //   Usage (Volume Increment)
    0x09, 0xEA,       //   Usage (Volume Decrement)
    
    0x75, 0x01,       // Report Size (1 bit) <-- Each key occupies 1 bit
    0x95, 0x05,       // Report Count (5)    <-- A total of 5 keys
    0x81, 0x02,       // Input (Data, Var, Abs) <-- Key data (Bit Field)

    //Pad the remaining bits to align the report to the byte boundary (3 bits of padding)
    0x75, 0x03,       // Report Size (3 bits) <-- Remaining 3 bits
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x03,       // Input (Const, Var, Abs) <-- Padding/constant data

    //Pad the second byte (optional, but recommended to use a multiple of bytes)
    0x75, 0x08,       //   Report Size (8 bits)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x03,       //   Input (Const, Var, Abs)

    0xC0              // End Collection
};

//below is usage defination,when Usage page=Consumer Page
//for details please refer to https://usb.org/document-library/hid-usage-tables-16
//https://www.usb.org/hid

#define HID_CONSUMER_VOLUME_UP      0xe9 // Volume Increment
#define HID_CONSUMER_VOLUME_DOWN    0xea // Volume Decrement
#define HID_CONSUMER_PLAY_PAUSE     0xcd // Play/Pause
#define HID_CONSUMER_SCAN_NEXT_TRACK     0xb5 // scan next track 
#define HID_CONSUMER_SCAN_PREVIOUS_TRACK     0xb6 // scan previous track 

#define HID_RPT_ID_CC_IN        3   // Consumer Control input report ID
#define HID_CC_IN_RPT_LEN       2   // Consumer Control input report Len


//report buffer use to store byte which be sent to ble hid host
uint8_t report_buffer[2] = {0, 0};


//Define the bitmask occupied by all media keys in the first data byte (s[0]).
#define HID_CC_ALL_MEDIA_BITS       0x1F  // 0001 1111b (Bit 0 - Bit 4)

//different consumer control occupy bitmask
#define HID_CC_VOLUME_BITS          0x18  // Bit 3 (Vol Up) & Bit 4 (Vol Down)
#define HID_CC_TRACK_BITS           0x03  // Bit 0 (Next) & Bit 1 (Prev)
#define HID_CC_PLAY_PAUSE_BIT       0x04  // Bit 2 (Play/Pause)



//set Volume Increment   Bit 3 (0x08)
#define HID_CC_RPT_SET_VOLUME_UP(s)     (s)[0] &= (~HID_CC_VOLUME_BITS); \
                                        (s)[0] |= 0x08
//release Volume Increment   Bit 3 (0x08)
#define HID_CC_RPT_RELEASE_VOLUME_UP(s)     (s)[0] &=(~ HID_CC_VOLUME_BITS);  


// set Volume Decrement  Bit 4 (0x10)
#define HID_CC_RPT_SET_VOLUME_DOWN(s)   (s)[0] &= (~HID_CC_VOLUME_BITS); \
                                        (s)[0] |= 0x10

// release Volume Decrement  Bit 4 (0x10)
#define HID_CC_RPT_RELEASE_VOLUME_DOWN(s)     (s)[0] &=(~ HID_CC_VOLUME_BITS);  

//set Play/Pause Bit 2 (0x04)
#define HID_CC_RPT_SET_PLAY_PAUSE(s)    (s)[0] &= (~HID_CC_PLAY_PAUSE_BIT); \
                                        (s)[0] |= 0x04

// release Play/Pause Bit 2 (0x04)
#define HID_CC_RPT_RELEASE_PLAY_PAUSE(s)    (s)[0] &= (~HID_CC_PLAY_PAUSE_BIT); 
                                     

// set Scan Next Track  Bit 0 (0x01)
#define HID_CC_RPT_SET_NEXT_TRACK(s)    (s)[0] &= (~HID_CC_TRACK_BITS); \
                                        (s)[0] |= 0x01

#define HID_CC_RPT_RELEASE_NEXT_TRACK(s)     (s)[0] &=(~ HID_CC_TRACK_BITS);

// set Scan Previous Track Bit 1 (0x02)
#define HID_CC_RPT_SET_PREV_TRACK(s)    (s)[0] &= (~HID_CC_TRACK_BITS); \
                                        (s)[0] |= 0x02

#define HID_CC_RPT_RELEASE_PREV_TRACK(s)     (s)[0] &=(~ HID_CC_TRACK_BITS);


//HID raw report map 
static esp_hid_raw_report_map_t ble_report_maps[] = {
    
    {
        .data = mediaReportMap,
        .len = sizeof(mediaReportMap)
    }

};


static esp_hid_device_config_t ble_hid_config = {
    .vendor_id          = 0x16C0,
    .product_id         = 0x05DF,
    .version            = 0x0100,

    .device_name        = "esphhhh",


    .manufacturer_name  = "Espressif",
    .serial_number      = "1234567890",
    .report_maps        = ble_report_maps,   //report  descriptor  array,may include many report descriptors
    .report_maps_len    = 1  ,               //the number of report  descriptor  array

    // If a device has multiple Application Collections or multiple interfaces, 
    //there may be multiple report maps.
    // For example, an all-in-one device with a keyboard and mouse (a combined HID device) 
    //may require two report maps.               

};  


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static void user_ble_start_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);


static void user_ble_pair_pass_entry_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data);


static void user_ble_close_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data);

                                           
static void user_ble_set_conn_handle_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data);

static void user_consumer_control_handler(void* arg, esp_event_base_t event_base,
int32_t event_id, void* event_data);



static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, 
                                           int32_t id, void *event_data);

void esp_hidd_send_consumer_control(uint8_t key_cmd, bool key_pressed);


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

#if CONFIG_BT_NIMBLE_ENABLED
/**
 * @brief BLE host task entry for NimBLE.
 *
 * This task runs the NimBLE host stack. It will return only when
 * nimble_port_stop() is executed. The function calls into the NimBLE
 * event loop and then deinitializes the FreeRTOS port on exit.
 *
 * @param param Unused parameter pointer passed by the task create API.
 */
void ble_hid_device_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}
void ble_store_config_init(void);
#endif





/**
 * @brief Register BLE-related event handlers with the UI event loop.
 *
 * Registers handlers for BLE start/close events, connection-handle updates
 * and consumer-control events so the application can react to BLE lifecycle
 * events originating from the UI/event system.
 */
void ble_register_srv_handler(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_START, user_ble_start_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_CLOSE, user_ble_close_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_SET_CONN_HANDLE, user_ble_set_conn_handle_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, 
                                APP_BLE_CONSUMER_CONTROL, user_consumer_control_handler, NULL));
}




/**
 * @brief Event handler invoked when BLE should be started.
 *
 * Initializes BLE GAP and HID device, configures advertising, sets up a
 * semaphore used during deinitialization, and starts the NimBLE host task.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base the event was posted to.
 * @param event_id Event identifier.
 * @param event_data Event-specific data (unused).
 */
static void user_ble_start_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data)
{
    esp_err_t ret;
    #if HID_DEV_MODE == HIDD_IDLE_MODE
        ESP_LOGE(TAG, "Please turn on BT HID device or BLE!");
        return;
    #endif

    // Initialize the Controller layer and BlueDroid host stack, register GAP event callback functions, and create semaphores.
    ret = esp_hid_gap_init(HID_DEV_MODE);
    ESP_ERROR_CHECK(ret);

    esp_bt_controller_status_t sta=esp_bt_controller_get_status();
    ESP_LOGW(TAG,"gap init ble controller status is %d",sta);

    //config Security Manager Protocol and register advertise call back function
    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, ble_hid_config.device_name);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "setting ble device");

    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev));

    //Due to the asynchronous callback nature of BLE events,
    //a deinitialization semaphore is created to prevent race conditions 
    //that could cause program crashes.
    s_ble_hid_param.deinit_sem = xSemaphoreCreateBinary();
    if (s_ble_hid_param.deinit_sem == NULL) 
    {
        ESP_LOGE(TAG, "Failed to create deinit semaphore");
        // Handle error
    }
    s_ble_hid_param.is_deinitializing = false;
    s_ble_hid_param.connected = false;


    ble_store_config_init();

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ret = esp_nimble_enable(ble_hid_device_host_task);
    if (ret!=ESP_OK) {
        ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
    }
    else
    {

        ESP_LOGI(TAG,"create ble_hid_device_host_task success");
    }
    


}



/**
 * @brief Event handler invoked to close/deinitialize BLE resources.
 *
 * Stops advertising, terminates any active connection (waiting for the
 * disconnect semaphore), resets GATT server state, deinitializes the HID
 * device, and deinitializes NimBLE and BLE controller resources.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base the event was posted to.
 * @param event_id Event identifier.
 * @param event_data Event-specific data (unused).
 */
static void user_ble_close_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data)
{
    // Stop advertising if active
    esp_err_t ret = ble_gap_adv_stop();
    if (ret != ESP_OK && ret != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop advertising: %d", ret);
    }

    // Terminate connection if active
    if (s_ble_hid_param.connected) 
    {
        s_ble_hid_param.is_deinitializing = true;
        ret = ble_gap_terminate(s_ble_hid_param.conn_handle, BLE_ERR_CONN_TERM_LOCAL);
        
        //terminate connection success or no current connection
        if (ret == 0||ret==BLE_HS_ENOTCONN) 
        {
            ESP_LOGW(TAG,"ble_gap_terminate ret=%d,pass in con_handle =%d",ret,s_ble_hid_param.conn_handle);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to terminate connection: %d", ret);
            s_ble_hid_param.is_deinitializing = false;
            return;  // Abort deinit
        }

        // Wait for disconnect event (timeout: 5 seconds)
        if (xSemaphoreTake(s_ble_hid_param.deinit_sem, 5000 / portTICK_PERIOD_MS) != pdTRUE) {
            ESP_LOGE(TAG, "Timeout waiting for disconnect");
            s_ble_hid_param.is_deinitializing = false;
            return;  // Abort deinit
        }
    }

    //reset gatt server
    ble_gatts_reset();

    
    if (s_ble_hid_param.hid_dev) 
    {
        //stop hid device 
        esp_hidd_dev_deinit(s_ble_hid_param.hid_dev);
        s_ble_hid_param.hid_dev = NULL;
    }

    esp_hid_gap_deinit();

    ret=nimble_port_stop();
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG, "nimble_port_stop fail ");
        return ;
    }

    ret=nimble_port_deinit();
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG, "nimble_port_deinit fail ");
        return ;
    }

    esp_bt_controller_status_t sta=esp_bt_controller_get_status();
    ESP_LOGI(TAG,"ble controller status is %d",sta);

    vSemaphoreDelete(s_ble_hid_param.deinit_sem);
    s_ble_hid_param.deinit_sem = NULL;

    ESP_LOGI(TAG, "BLE HID deinit complete");
    
}



/**
 * @brief Event handler to update stored BLE connection handle.
 *
 * Expects event_data to point to a uint16_t containing the new connection
 * handle value which is stored in the local state for later operations.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base the event was posted to.
 * @param event_id Event identifier.
 * @param event_data Pointer to a uint16_t holding the connection handle.
 */
static void user_ble_set_conn_handle_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data)
{
    if(event_data!=NULL)
    {
        s_ble_hid_param.conn_handle=*((uint16_t *)event_data);

        ESP_LOGI(TAG,"now conn handle=%d",s_ble_hid_param.conn_handle);

    }

}


/**
 * @brief UI event handler that sends consumer control HID reports.
 * Receive the BLE consumer control events generated by clicking the button widget.
 * The handler reads a consumer control usage code from event_data and sends
 * a press followed by a short delay and a release report to the connected
 * HID host to emulate a button click.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base the event was posted to.
 * @param event_id Event identifier.
 * @param event_data Pointer to a uint8_t containing the consumer control code.
 */
static void user_consumer_control_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data)
{
    if(event_data!=NULL)
    {
        uint8_t control=*((uint8_t *)event_data);
        esp_hidd_send_consumer_control(control,true);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_hidd_send_consumer_control(control,false);

    }
}


/**
 * @brief Callback that handles HID device events from esp_hidd.
 *
 * Handles HID lifecycle events (start, connect, protocol mode changes,
 * control, output, feature, disconnect and stop). Updates local connection
 * state, triggers advertising and posts application-level events when needed.
 *
 * @param handler_args Callback-specific arguments (opaque).
 * @param base Event base from esp_hidd.
 * @param id Event id corresponding to esp_hidd_event_t.
 * @param event_data Pointer to event-specific data (esp_hidd_event_data_t*).
 */
static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    

    switch (event) 
    {
    case ESP_HIDD_START_EVENT: 
    {
        ESP_LOGI(TAG, "START");

        //start advertise
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
        ESP_LOGI(TAG, "CONNECT");
        
        //indicate success build connection
        s_ble_hid_param.connected = true;

        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT: {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT: {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        if (param->control.control)
        {

        } else 
        {

        }
    break;
    }
    case ESP_HIDD_OUTPUT_EVENT: {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
        break;
    }
    case ESP_HIDD_FEATURE_EVENT: {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));

        s_ble_hid_param.connected = false;

        //judge whether in progress of deiniting
        if (s_ble_hid_param.is_deinitializing) 
        {
            xSemaphoreGive(s_ble_hid_param.deinit_sem);
            ESP_LOGW(TAG,"give deinit sema success");
            s_ble_hid_param.is_deinitializing = false;
        }
        //post ble connection close  event 
        ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_CONNECTION_CLOSE, NULL, 0, portMAX_DELAY));

        esp_hid_ble_gap_adv_start();


        break;
    }
    case ESP_HIDD_STOP_EVENT: {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}



/**
 * @brief Update and send a consumer control HID input report.
 *
 * This function adjusts the global report_buffer based on the requested
 * consumer control key (volume, track, play/pause) and whether the key is
 * being pressed or released, then sends the input report to the HID host.
 *
 * @param key_cmd Consumer control usage code (see HID_CONSUMER_* defines).
 * @param key_pressed True to send a press report, false to send a release.
 */
void esp_hidd_send_consumer_control(uint8_t key_cmd, bool key_pressed)
{
    
    if (key_pressed) 
    {
        switch (key_cmd) 
        {
        case HID_CONSUMER_VOLUME_UP:
            ESP_LOGI(TAG, "Send the volume up");
            HID_CC_RPT_SET_VOLUME_UP(report_buffer);
            break;

        case HID_CONSUMER_VOLUME_DOWN:
            ESP_LOGI(TAG, "Send the volume down");
            HID_CC_RPT_SET_VOLUME_DOWN(report_buffer);
            break;

        case HID_CONSUMER_PLAY_PAUSE:
            ESP_LOGI(TAG, "Send the play/pause");
            HID_CC_RPT_SET_PLAY_PAUSE(report_buffer);
            break;

        case HID_CONSUMER_SCAN_NEXT_TRACK:
            ESP_LOGI(TAG, "Send the next track");
            HID_CC_RPT_SET_NEXT_TRACK(report_buffer);
            break;

        case HID_CONSUMER_SCAN_PREVIOUS_TRACK:
            ESP_LOGI(TAG, "Send the prev track");
            HID_CC_RPT_SET_PREV_TRACK(report_buffer);
            break;

 
        default:
            break;
        }
    }
    else
    {         
        switch (key_cmd) 
        {
        case HID_CONSUMER_VOLUME_UP:

        case HID_CONSUMER_VOLUME_DOWN:
            ESP_LOGI(TAG, "release the volume operation");
            HID_CC_RPT_RELEASE_VOLUME_UP(report_buffer);
            break;

        case HID_CONSUMER_PLAY_PAUSE:
            ESP_LOGI(TAG, "release paly/pause");
            HID_CC_RPT_RELEASE_PLAY_PAUSE(report_buffer);
            break;

        case HID_CONSUMER_SCAN_NEXT_TRACK:

        case HID_CONSUMER_SCAN_PREVIOUS_TRACK:
            ESP_LOGI(TAG, "release the track operation");
            HID_CC_RPT_RELEASE_PREV_TRACK(report_buffer);
            break;

 
        default:
            break;
        }
    }

    //send data
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, HID_RPT_ID_CC_IN, report_buffer, HID_CC_IN_RPT_LEN);
    return;
}