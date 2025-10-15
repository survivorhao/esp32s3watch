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
    bool connected;             // Add: Connection status flag
    uint16_t conn_handle;       // Add: BLE connection handle
    SemaphoreHandle_t deinit_sem; // Add: Semaphore to wait for disconnect during deinit
    bool is_deinitializing;     // Add: Flag to signal intentional deinit
} local_param_t;


static local_param_t s_ble_hid_param = {0};

const unsigned char mediaReportMap[] = {

    // --- 顶级集合 (Report ID 3: Media Control) ---
    0x05, 0x0C,       // Usage Page (Consumer Devices - 0x0C)
    0x09, 0x01,       // Usage (Consumer Control - 0x01)
    0xA1, 0x01,       // Collection (Application)

    0x85, 0x03,       // Report ID (3)  <-- 使用 Report ID 3

    // 1. 定义 5 个媒体按键的输入字段 (Bit Field)
    0x15, 0x00,       //   Logical Minimum (0)  <-- 0 表示未按下
    0x25, 0x01,       //   Logical Maximum (1)  <-- 1 表示按下

    // 使用 Usage Minimum/Maximum 范围来一次性定义 5 个按键
    // 确保您定义的这 5 个 Usage ID 顺序是连续的，或者通过多个 Usage Item 定义
    
    // 方法一：精确定义您需要的 5 个按键（Bit 0 - Bit 4）
    0x09, 0xB5,       //   Usage (Scan Next Track)
    0x09, 0xB6,       //   Usage (Scan Previous Track)
    0x09, 0xCD,       //   Usage (Play/Pause)
    0x09, 0xE9,       //   Usage (Volume Increment)
    0x09, 0xEA,       //   Usage (Volume Decrement)
    
    0x75, 0x01,       //   Report Size (1 bit) <-- 每个按键占用 1 位
    0x95, 0x05,       //   Report Count (5)    <-- 共有 5 个按键
    0x81, 0x02,       //   Input (Data, Var, Abs) <-- 按键数据 (Bit Field)

    // 2. 填充剩余的位，使报告对齐到字节边界（3位填充）
    0x75, 0x03,       //   Report Size (3 bits) <-- 剩余 3 位
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x03,       //   Input (Const, Var, Abs) <-- 填充/常量数据

    // 3. 填充第二个字节 (可选，但推荐使用整数倍的字节)
    0x75, 0x08,       //   Report Size (8 bits)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x03,       //   Input (Const, Var, Abs)

    0xC0              // End Collection
};


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
    .report_maps        = ble_report_maps,         //report  descriptor  array,可以包含多个report descriptor
    .report_maps_len    = 1  ,                      //report  descriptor  array中的元素数量

    //如果设备有多个应用集合（Application Collection）或多个接口，就可能有多个 report map。
    // 比如键盘 + 鼠标一体机（组合 HID 设备），就可能需要两个 report map                  



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


static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, 
                                           int32_t id, void *event_data);

void ble_hid_task_start_up(void);

void ble_hid_task_shut_down(void);

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

#if CONFIG_BT_NIMBLE_ENABLED
void ble_hid_device_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}
void ble_store_config_init(void);
#endif





void ble_register_srv_handler(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, 
                                        APP_BLE_START, user_ble_start_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, 
                                        APP_BLE_CLOSE, user_ble_close_handler, NULL));

    
}




static void user_ble_start_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data)
{
    esp_err_t ret;
    #if HID_DEV_MODE == HIDD_IDLE_MODE
        ESP_LOGE(TAG, "Please turn on BT HID device or BLE!");
        return;
    #endif

    //初始化  Controller layer and BlueDroid  host  stack and 注册gap事件 Call Back 函数 and   创建信号量
    ret = esp_hid_gap_init(HID_DEV_MODE);
    ESP_ERROR_CHECK( ret );

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



static void user_ble_close_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data)
{
    // Stop advertising if active
    esp_err_t ret = ble_gap_adv_stop();
    if (ret != ESP_OK && ret != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop advertising: %d", ret);
    }

    // Terminate connection if active
    if (s_ble_hid_param.connected) {
        s_ble_hid_param.is_deinitializing = true;
        ret = ble_gap_terminate(s_ble_hid_param.conn_handle, BLE_ERR_CONN_TERM_LOCAL);
        if (ret == 0||ret==7) 
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
    ESP_LOGW(TAG,"ble controller status is %d",sta);

    // ret=esp_bt_controller_disable();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "disable ble controller fail %d", ret);
    // }


    // esp_bt_controller_deinit();

    vSemaphoreDelete(s_ble_hid_param.deinit_sem);
    s_ble_hid_param.deinit_sem = NULL;

    ESP_LOGI(TAG, "BLE HID deinit complete");
    
}



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
        // ble_hid_task_start_up();
        
        //indicate success build connection
        s_ble_hid_param.connected = true;
        if (xSemaphoreTake(ble_con_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) 
        {
            ESP_LOGE(TAG, "take ble_con_mutex timeout ");
            return ;

        }
        s_ble_hid_param.conn_handle=ble_get_conn_handle();
        xSemaphoreGive(ble_con_mutex);

        ESP_LOGW(TAG,"set con_handle=%d",s_ble_hid_param.conn_handle);
        

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
            // exit suspend
            ble_hid_task_start_up();
        } else {
            // suspend
            ble_hid_task_shut_down();
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
        // ble_hid_task_shut_down();
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


void ble_hid_demo_task(void *pvParameters)
{
   
    uint8_t  data_bufer=0;
    uint8_t  temp_data=100;
    while (1) 
    {
        vTaskDelay(100000);

        if(data_bufer==1)
        {
            
            // ESP_LOGI("mytset","-------------------button pressed message send to hid task,");
            // esp_hidd_send_consumer_value(HID_CONSUMER_PLAY,true);
        
            vTaskDelay(10);

            // esp_hidd_send_consumer_value(HID_CONSUMER_PLAY,false);

            ESP_LOGI("mytest","");


        }

    }
}

void ble_hid_task_start_up(void)
{
    if (s_ble_hid_param.task_hdl) {
        // Task already exists
        return;
    }
    ESP_LOGI(TAG, "create hid task ");

    xTaskCreate(ble_hid_demo_task, "ble_hid_demo_task", 3 * 1024, NULL, configMAX_PRIORITIES - 3,
                &s_ble_hid_param.task_hdl);


}

void ble_hid_task_shut_down(void)
{
    if (s_ble_hid_param.task_hdl) {
        vTaskDelete(s_ble_hid_param.task_hdl);
        s_ble_hid_param.task_hdl = NULL;
    }
}
