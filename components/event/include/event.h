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

    APP_WIFI_DISCONNECTED,                  // WiFi connection disconnected

    APP_GET_IP,                              //successfully get ip

    APP_WEATHER_REQUEST,                   //request weather

    APP_WEATHER_RESPONSE,                  //weather   event  response 

    APP_WEATHER_POSITION_CHANGE,           //weather   event  response 

    APP_SNTP_REQUEST,                      //sntp service request       

    APP_SNTP_SYNED,                        //sntp syned          

    APP_FILE_REFRESH_REQUEST,               //file list refresh request

    APP_FILE_REFRESH_RESPONSE,

    //camera init 
    APP_CAMERA_ENTER,

    //camera deinit 
    APP_CAMERA_EXIT,

    APP_CAMERA_PIC_DELETE,              //delete pic in sdcard which saved by camera

    APP_BLE_START,                      //start ble hid 

    APP_BLE_PAIR_PASSKEY_ENTRY,
    
    APP_BLE_PAIR_SUCCESS,

    APP_BLE_CONNECTION_CLOSE,
    
    APP_BLE_SET_CONN_HANDLE,         //set ble connection handle

    APP_BLE_CONSUMER_CONTROL,


    APP_BLE_CLOSE,

};



// Define the Wi-Fi list data used for passing in events
typedef struct {
    uint16_t ap_count;
    wifi_ap_record_t *ap_records;
} wifi_scan_done_data_t;

// Define data for Wi-Fi connection requests to be passed in events.
typedef struct {
    char ssid[33];
    char password[64];
} wifi_connect_data_t;



// The weather data includes today's weather data and three-day weather data.
typedef struct
{
    // Position
    char location[32];

    //today temperature
    char  today_tem[8];

    // Today's weather string
    char today_weather[16];

    // Today's weather code
    int today_weather_code;

    // Three-day temperature range data, in order: today / tomorrow / the day after tomorrow
    char  _3day_tem_range[3][8];
    
    // Three-day weather code, in order: Today / Tomorrow / The day after tomorrow
    int  _3day_weather[3];


}weather_data_t;


//file list refresh request 
typedef struct 
{
    // 1. Table of Contents
    bool is_directory;

    // The name in the List
    char name[32];

    // Current sd path
    char current_path[64];

    char ext_name[16];


} file_refresh_req_data_t;



#define  SD_PATH_MAX  128

// file item struct
typedef struct 
{
    char name[32];
    bool is_dir;       //whether is a directory or file ,1 mean directory
    
} file_item_t;


//file list refresh response
typedef struct 
{
    char current_path[SD_PATH_MAX];
    file_item_t *items;   // Dynamic Array
    uint8_t  item_count;
    esp_err_t err;        // Error code
} file_refresh_res_data_t;





/**
 * @brief Create and configure the UI event loop used for inter-component events.
 *
 * This function creates a dedicated FreeRTOS-based esp_event loop for
 * application UI events and stores the resulting handle in
 * `ui_event_loop_handle`. 
 * Other modules should post events to this loop to decouple event producers from
 * consumers.
 * 
 */
void  ui_event_loop_init(void);















