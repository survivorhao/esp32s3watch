//system include file
#include    <stdio.h>
#include    "esp_log.h"
#include    "freertos/FreeRTOS.h"
#include    "freertos/task.h"
#include    <stdint.h>
#include    <inttypes.h>
#include    "nvs_flash.h"
#include    "esp_sleep.h"
#include    "esp_pm.h"
#include    "driver/gpio.h"




//custom inlcude file
#include    "sdcard.h"
#include    "io_extend.h"
#include    "bsp_driver.h"
#include    "camera.h"
#include    "my_ui.h"
#include    "ui.h"
#include    "event.h"
#include    "wifi_connect.h"
#include    "time_srv.h"
#include    "weather_srv.h"
#include    "esp_camera.h"
#include    "ble_srv.h"
#include    "esp_hid_gap.h"


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static const char * TAG="my_main";



//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//
void app_lvgl_display(void);

void nvs_user_data_init(void);

//init save camera frame sequence
void nvs_set_camera_frame_sequence(void);

//init http client weatehr position 
void nvs_weather_position_init(void);


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
// app_main
//----------------------------------------------------------------------------------------------------------------------------



void app_main(void)
{  
    // Initialize RTC flag used to indicate whether local time has been
    // synchronized across boots/deep-sleep. This flag influences whether
    // the system should perform an SNTP time synchronization later.
    rtc_local_time_syned_flag_init();

    // Initialize NVS (non-volatile storage). If the partition layout
    // changed or there are no free pages, erase and re-init to recover.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // Load or create application-specific persistent data in NVS
    // (e.g. camera frame sequence counter, weather position config).
    nvs_user_data_init();

    // Initialize I2C bus and attach devices used by the system
    // (OLED display, IO extender, etc.).
    bsp_i2c_driver_init();

    // Configure hardware buttons and register GPIO interrupt handlers.
    bsp_button_init();

    // Initialize IO extender (PCA9557) and set its default output levels.
    io_extend_init();

    // Initialize LVGL port, display driver and touch input. Aborts on error.
    ESP_ERROR_CHECK(app_lvgl_init());

    // Set display rotation for LVGL (rotate framebuffer by 90 degrees).
    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_90);

    // Create the application's UI event loop used to post and handle UI events.
    ui_event_loop_init();

    // Mount SD card filesystem at /sdcard so files can be read and written.
    sdcard_init_mount_fs();
    
    // Register SNTP request handler so UI or other modules can trigger time sync.
    time_register_sntp_handler();

    // Register weather service handlers for fetching and processing weather data.
    weather_register_weathe_service_handler();

    // Register camera-related handlers for init/deinit and camera events.
    camera_register_camera_handler();

    // Register BLE/HID service handlers so BLE features are available.
    ble_register_srv_handler();

    // Initialize and show the initial LVGL UI screens/widgets.
    app_lvgl_display();

    // Create and pin the WiFi management task on core 1; it handles WiFi
    // state machine and system WiFi events.
    xTaskCreatePinnedToCore(wifi_task,"my_wifi_task",4096,NULL,1,NULL,1 );

    // Create and pin the UI task on core 1; it runs LVGL handling and
    // processes UI-related events with higher priority.
    xTaskCreatePinnedToCore(my_ui_task,"my_ui_task",8192,NULL,4,NULL,1 );
    

    

}
// *INDENT-OFF*
void app_lvgl_display(void)
{
    lvgl_port_lock(0);

    ui_init();

    lvgl_port_unlock();
}

/// @brief Initialize or read the corresponding data in NVS.
/// @param  
void nvs_user_data_init(void)
{
    //camera frame sequence set
    nvs_set_camera_frame_sequence();

    nvs_weather_position_init();

}

/// @brief 
/// @param  
void nvs_set_camera_frame_sequence(void)
{

    nvs_handle_t handle;
    esp_err_t err = nvs_open("user", NVS_READWRITE, &handle);
    if (err != ESP_OK) 
    {
       ESP_LOGE(TAG,"nvs open namespace fail ");
       return ;
    }

    err = nvs_get_i32(handle, "frame_seq", (int32_t*)&camera_frame_save_file_counter);
    if (err == ESP_ERR_NVS_NOT_FOUND) 
    {
        // key 不存在，写入初始值0 
        nvs_set_i32(handle, "frame_seq", 0);
        nvs_commit(handle);

    }else if (err == ESP_OK) 
    {

    }else 
    {
        ESP_LOGE(TAG,"nvs namespace read specific key fail ");
        return ;
    }

    nvs_close(handle);


}
/// @brief read weather position configuration in nvs
/// @param  
void nvs_weather_position_init(void)
{

    nvs_handle_t handle;
    esp_err_t err = nvs_open("user", NVS_READWRITE, &handle);
    if (err != ESP_OK) 
    {
       ESP_LOGE(TAG,"nvs open namespace fail ");
       return ;
    }

    size_t required_size;
    err = nvs_get_str(handle, "weather_pos", NULL, &required_size);
    
    if(required_size >sizeof(weather_position))
    {
        ESP_LOGE(TAG, "require size > actual weather_position array  size");
        return ;

    }
    err = nvs_get_str(handle, "weather_pos", weather_position, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) 
    {
        // key 不存在，写入初始值0 
        nvs_set_str(handle, "weather_pos", weather_position);
        nvs_commit(handle);

    }else if (err == ESP_OK) 
    {

    }else 
    {
        ESP_LOGE(TAG,"nvs namespace read specific key fail ");
        return ;
    }

    nvs_close(handle);

}

