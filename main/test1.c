#include    <stdio.h>
#include    "esp_log.h"
#include    "freertos/FreeRTOS.h"
#include    "freertos/task.h"
#include    <stdint.h>
#include    <inttypes.h>
#include    "nvs_flash.h"

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


//输出任务信息
static void print_task_list_vTaskList(void);

//输出heap信息
static void my_test_printf_hesp_info(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data) ;

void print_heap_caps_stats(void);



//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------
void lv_log_print(const char * buf)
{
    printf("%s\n",buf);


}


void app_main(void)
{  

    // 初始化 NVS（全局，一次性）
    ESP_ERROR_CHECK(nvs_flash_init());
    
    nvs_user_data_init();
    
    print_heap_caps_stats();

    //初始化 新版i2c驱动
    bsp_i2c_driver_init();    

    bsp_button_init();

    //init io extend chip
    io_extend_init();

    ESP_ERROR_CHECK(app_lvgl_init());

    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_90);

    ui_event_loop_init();

    sdcard_init_mount_fs();
    
    app_lvgl_display();

    time_register_sntp_handler();

    weather_register_weathe_service_handler();

    camera_register_camera_handler();

    // print_heap_caps_stats();

    xTaskCreatePinnedToCore(wifi_task,"my_wifi_task",8192,NULL,1,NULL,1 );

    // print_heap_caps_stats();

    xTaskCreatePinnedToCore(my_ui_task,"my_ui_task",8192,NULL,4,NULL,1 );
    






    // 订阅UI任务发布的事件
    // ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_SCAN_START, &my_test_printf_hesp_info, NULL));
    // ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_GET_IP, &my_test_printf_hesp_info, NULL));
   
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WEATHER_REQUEST, &my_test_printf_hesp_info, NULL));


    // print_heap_caps_stats();

    // vTaskDelay(800);            //delay 10s
    // print_task_list_vTaskList();
    // vTaskDelay(NULL);

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




static void print_task_list_vTaskList(void)
{
    // 根据实际任务数与名字长度调整 buffer 大小
    static char *buf = NULL;
    const size_t buf_size = 1024;

    if (!buf) {
        buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buf) {
            ESP_LOGE(TAG, "malloc fail for vTaskList buffer");
            return;
        }
    }
    // Format: "Name          State   Prio    Stack   Num\n"
    vTaskList(buf);
    ESP_LOGI(TAG, "===== vTaskList() =====\nName          State   Prio    Stack   Num\n%s", buf);
    free(buf);
    buf=NULL;
}



// 关闭wifi
static void my_test_printf_hesp_info(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data) 
{

    print_heap_caps_stats();
    print_task_list_vTaskList();


}

void print_heap_caps_stats(void)
{
    multi_heap_info_t info;

    // Default heap
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    printf("\n Default Heap:\n");
    printf("  Total size      : %u\n", (unsigned)heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
    printf("  Free size       : %u\n", (unsigned)info.total_free_bytes);

    // Internal RAM heap
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    printf("Internal RAM Heap:\n");
    printf("  Total size      : %u\n", (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    printf("  Free size       : %u\n", (unsigned)info.total_free_bytes);

    // DMA-capable heap
    heap_caps_get_info(&info, MALLOC_CAP_DMA);
    printf("DMA-capable Heap:\n");
    printf("  Total size      : %u\n", (unsigned)heap_caps_get_total_size(MALLOC_CAP_DMA));
    printf("  Free size       : %u\n", (unsigned)info.total_free_bytes);
}
