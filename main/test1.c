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



#include    "sdcard.h"
#include    "audio.h"
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
//----------------------------------------------------------------------------------------------------------------------------



void app_main(void)
{  
    //init rtc_local_time_syned_flag to judge whether to send sntp request
    rtc_local_time_syned_flag_init();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    nvs_user_data_init();
    

    //初始化 新版i2c驱动
    bsp_i2c_driver_init();    

    bsp_button_init();

    //init io extend chip
    io_extend_init();

    ESP_ERROR_CHECK(app_lvgl_init());

    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_90);

    ui_event_loop_init();

    sdcard_init_mount_fs();
    
    time_register_sntp_handler();

    weather_register_weathe_service_handler();

    camera_register_camera_handler();

    ble_register_srv_handler();

    app_lvgl_display();

    xTaskCreatePinnedToCore(wifi_task,"my_wifi_task",4096,NULL,1,NULL,1 );

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

    err = nvs_get_i32(handle, "frame_seq", &camera_frame_save_file_counter);
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

