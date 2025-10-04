#pragma once
#include "esp_event.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_err.h"



extern esp_event_loop_handle_t   ui_event_loop_handle;


/* APP_EVENT  declaration  */
ESP_EVENT_DECLARE_BASE(APP_EVENT); 



//ui event id
enum{

    APP_WIFI_SCAN_START=0x00,        //start  wifi  scan

    APP_WIFI_SCAN_DONE,              //wifi scan done

    APP_WIFI_CLOSE,                  //close wifi

    APP_WIFI_CONNECT_REQUEST,                //connect wifi request 

    APP_WIFI_CONNECTED,                      //wifi successfully connected    

    APP_WIFI_DISCONNECTED,                  //wifi连接断开

    APP_GET_IP,                              //successfully get ip

    APP_WEATHER_REQUEST,                   //request weather

    APP_WEATHER_RESPONSE,                  //weather   event  response 

    APP_SNTP_REQUEST,                      //sntp service request       

    APP_SNTP_SYNED,                        //sntp syned          

    APP_FILE_REFRESH_REQUEST,               //file list refresh request

    APP_FILE_REFRESH_RESPONSE,

    //camera init 
    APP_CAMERA_ENTER,

    //camera deinit 
    APP_CAMERA_EXIT,


};



// 定义用于在事件中传递的Wi-Fi列表数据
typedef struct {
    uint16_t ap_count;
    wifi_ap_record_t *ap_records;
} wifi_scan_done_data_t;

// 定义用于在事件中传递的Wi-Fi连接请求数据
typedef struct {
    char ssid[33];
    char password[64];
} wifi_connect_data_t;



//天气数据包含今天天气数据 和 三天天气数据
typedef struct
{
    //位置
    char location[32];

    //today temperature
    char  today_tem[8];

    //今天天气字符串
    char today_weather[16];

    //今天天气code
    int today_weather_code;

    //三天温度范围数据，依次为 今天 / 明天 /后天
    char  _3day_tem_range[3][8];
    
    //三天天气code，依次为 今天 / 明天 /后天
    int  _3day_weather[3];


}weather_data_t;


//file list refresh request 
typedef struct 
{
    //1,目录
    bool is_directory;

    //List中的name
    char name[32];

    //当前sd路径
    char current_path[64];

    char ext_name[16];


} file_refresh_req_data_t;



#define  SD_PATH_MAX  128

// 文件项结构体（FS -> UI）
typedef struct 
{
    char name[32];
    bool is_dir;
    
} file_item_t;


//file list refresh response
typedef struct 
{
    char current_path[SD_PATH_MAX];
    file_item_t *items;   // 动态数组
    uint8_t  item_count;
    esp_err_t err;        // 错误码
} file_refresh_res_data_t;






void  ui_event_loop_init(void);















