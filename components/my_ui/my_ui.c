#include <stdio.h>
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <sys/time.h>
#include <stdio.h>

#include "esp_lvgl_port.h"
#include "my_ui.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_clock.h"
#include "ui_lockScreen.h"
#include "ui_wifi.h"
#include "custom_event.h"
#include "wifi_connect.h"



//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static const char *TAG="my_ui";

static lv_obj_t *wifi_list = NULL;  // WiFi AP List Widget

static lv_obj_t *ui_PasswordScreen = NULL;  // Password Input Screen

static lv_obj_t *ui_PasswordTextarea = NULL;  // Password input field

static lv_obj_t *ui_Keyboard = NULL;  // Keyboard

static lv_obj_t *ui_ConnectButton = NULL;  // Connect button

static lv_obj_t  *ui_ip_label=NULL;     //use this to diaplay get ip address

static lv_obj_t  *ui_ble_pass_label=NULL;       //display ble pair PIN

static char current_ssid[33] = {0};  // Temporarily store the clicked SSID




typedef enum {
    //start wifi and scan 
    UI_MSG_WIFI_START,
    
    //close wifi
    UI_MSG_WIFI_STOP,
    
    //scan done 
    UI_MSG_WIFI_SCAN_DONE,
    
    //wifi password input event 
    UI_MSG_WIFI_PASS_IN,

    //finish wifi password input 
    UI_MSG_WIFI_PASS_IN_DONE,

    //successfully get ip address
    UI_MSG_WIFI_GET_IP,

    //local time synchronized successfully  // change ui second widget per second event 
    UI_MSG_SNTP_SYNED,

    //successfully get weather information
    UI_MSG_WEATHER_RES,
    
    // WiFi connection failed or disconnected
    UI_MSG_WIFI_DISCONNECTED,     
    
    //refresh specific path response data
    UI_MSG_SD_REFRESH_RES,
    
    //ble pair password 
    UI_MSG_BLE_PAIR_PASS_ENTRY,

    //ble pair successfully
    UI_MSG_BLE_PAIR_SUCCESS,

    //ble connection close
    UI_MSG_BLE_CONNECTION_CLOSE,


} ui_message_type_t;


// Define the message structure for the UI task.
typedef struct {
    ui_message_type_t type;
    void *data; // Use a generic pointer to pass data.
} ui_message_t;

//ui task message queue
static QueueHandle_t ui_message_queue;


//max display item in specific directory
#define  SD_REFRESH_FILE_COUNT_MAX     35

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static void wifi_start_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);

static void wifi_stop_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);
                              
static void wifi_scan_done_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);                              

static void wifi_get_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void sntp_syned_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void weather_response_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void wifi_disconnected_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void sd_refresh_response_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void ble_pair_pass_entry_res_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void ble_pair_success_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void ble_connection_close_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);


static void ap_button_click_cb(lv_event_t *e);

void my_ui_create_password_screen(void);

static void connect_button_click_cb(lv_event_t *e);

// Convert the current time from the time structure into a string formatted as %02d:%02d:%02d for easy display on the screen.
void tm_to_string(const struct tm *tm_data, char *buf);

static void ui_weather_change_img(int weather_code, lv_obj_t **obj);

static void ui_weather_update(weather_data_t data);





//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------




void my_ui_task(void *par)
{
    // 1. Create a message queue
    ui_message_queue = xQueueCreate(10, sizeof(ui_message_t));
    if (ui_message_queue == NULL) 
    {
        ESP_LOGE(TAG, "Failed to create UI message queue");
        vTaskDelete(NULL);
    }    

    //wifi start ,begin scan 
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_WIFI_SCAN_START, 
                                wifi_start_handler, NULL));
    
    //wifi stop
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_WIFI_CLOSE, 
                                wifi_stop_handler, NULL));
    
    //wifi scan done
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_WIFI_SCAN_DONE, 
                                wifi_scan_done_handler, NULL));

    //successfully get ip address
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_GET_IP, 
                                wifi_get_ip_handler, NULL));

    //local time  successfully synchronized
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_SNTP_SYNED, 
                                sntp_syned_handler, NULL));

    //get ip address successfully
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_WEATHER_RESPONSE, 
                                weather_response_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_WIFI_DISCONNECTED, 
                                wifi_disconnected_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_FILE_REFRESH_RESPONSE, 
                                sd_refresh_response_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_BLE_PAIR_PASSKEY_ENTRY, 
                                ble_pair_pass_entry_res_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_BLE_PAIR_SUCCESS, 
                                ble_pair_success_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle,APP_EVENT, APP_BLE_CONNECTION_CLOSE, 
                                ble_connection_close_handler, NULL));


    ui_message_t received_msg;
    while (1) 
    {
        // Blocking wait for message
        if (xQueueReceive(ui_message_queue, &received_msg, portMAX_DELAY) == pdTRUE) 
        {
            // Lock to safely call the LVGL API, ensuring the LVGL API is thread-safe.
            if (lvgl_port_lock(0) == true) 
            {
                
                switch (received_msg.type) 
                {
                    case UI_MSG_WIFI_START:
                        
                        
                        // Hide the ui_wifi_off_image picture.
                        lv_obj_add_flag(ui_WifiOffImage, LV_OBJ_FLAG_HIDDEN);      // Flags

                        // Display the ui_wifi_on_image picture.
                        lv_obj_clear_flag(ui_WIfiOnImage, LV_OBJ_FLAG_HIDDEN);     /// Flags
                        break;
                    case UI_MSG_WIFI_STOP:
                        
                        // Hide the ui_wifi_on_image picture.
                        lv_obj_add_flag(ui_WIfiOnImage, LV_OBJ_FLAG_HIDDEN);      // Flags

                        // Display the ui_wifi_off_image picture.
                        lv_obj_clear_flag(ui_WifiOffImage, LV_OBJ_FLAG_HIDDEN);     /// Flags

                        if(ui_ip_label!=NULL)
                        {
                            lv_obj_del_delayed(ui_ip_label,100);
                            ui_ip_label=NULL;
                        }

                        if(wifi_list!= NULL)
                        {
                            lv_obj_del_delayed(wifi_list,200);

                            // Set wifi_list to NULL, and recreate it the next time scan is done.
                            wifi_list=NULL;
                        }


                        break;
                    case UI_MSG_WIFI_SCAN_DONE:


                        ESP_LOGI(TAG, "Received scan results, updating UI list...");

                        wifi_scan_done_data_t *ap_list=(wifi_scan_done_data_t* )received_msg.data;
                        if (ap_list == NULL || ap_list->ap_count == 0) 
                        {
                            ESP_LOGW(TAG, "No APs found or invalid data");
                            // Optional: Display a "No WiFi found" label in the UI
                            break;
                        }

                        // Create or update the list widget (assuming ui_WifiScreen is the WiFi screen object)
                        if (wifi_list == NULL) 
                        {
                            
                            wifi_list = lv_list_create(ui_wifi);        // Created on the WiFi screen

                            lv_obj_set_size(wifi_list, LV_PCT(100), LV_PCT(60));  // Sample size: full width, 80% height
                            
                            lv_obj_set_align(wifi_list, LV_ALIGN_CENTER);  // Centering
            
                            lv_obj_set_pos(wifi_list,0,55);

                            lv_obj_set_style_bg_color(wifi_list, lv_color_hex(0xffffff), LV_PART_MAIN);  
                            lv_obj_set_style_bg_opa(wifi_list, 255, LV_PART_MAIN);  
                            lv_obj_set_style_border_color(wifi_list, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_opa(wifi_list, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            
                        }else 
                        {
                            // If the list already exists, clear the old contents.
                            lv_obj_clean(wifi_list);
                        }

                        // Iterate through ap_records and add buttons.
                        for (uint8_t i = 0; i < ap_list->ap_count; i++) 
                        {
                            // Add button: icon LV_SYMBOL_WIFI + SSID
                            // lv_list_add_btn returns lv_obj_t * (button object)
                            lv_obj_t *btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WIFI, (const char *)ap_list->ap_records[i].ssid);
                            
                            lv_obj_set_style_bg_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);  
                            lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN); 
                            
                            if (btn != NULL) 
                            {
                                // Register click event
                                lv_obj_add_event_cb(btn, ap_button_click_cb, LV_EVENT_CLICKED, NULL);
                                
                            }else 
                            {
                                ESP_LOGE(TAG, "Failed to add list button for AP %d", i);
                            }
                        }

                        free(ap_list->ap_records);
                        free(received_msg.data);
                        break;

                        
                        //switch  current  screen  to  wifi  password  input screen 
                        case UI_MSG_WIFI_PASS_IN:
                            if (ui_PasswordScreen == NULL)
                            {
                                my_ui_create_password_screen();
                                                          
                            }
                            _ui_screen_change(&ui_PasswordScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 150, 0, NULL);      
                          
                        break;
                        
                        //return wifi screen 
                        case UI_MSG_WIFI_PASS_IN_DONE:

                            //return wifi screen 
                            _ui_screen_change(&ui_wifi, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 150, 0, NULL);  
                            lv_textarea_set_text(ui_PasswordTextarea,"");


                        break;

                        // After successfully getting the IP, delete the wifi_list and the password input screen.
                        case UI_MSG_WIFI_GET_IP:
                            
                            //lv_obj_del api
                            lv_obj_del_delayed(wifi_list,150);

                            // Set wifi_list to NULL, and recreate it the next time scan is done.
                            wifi_list=NULL;

                            // after delay some time, then  delete PassWord Screen 
                            lv_obj_del_delayed(ui_PasswordScreen,300);
                            
                            ui_PasswordScreen=NULL;

                            ESP_LOGI(TAG, "Received IP address, updating UI...");
                            if(ui_ip_label==NULL)
                            {                
                                ui_ip_label = lv_label_create(ui_wifi);
                                lv_obj_align(ui_ip_label, LV_ALIGN_CENTER, 0, 50);
                                lv_obj_set_size(ui_ip_label,240,120);
                                lv_obj_set_style_text_font(ui_ip_label,&lv_font_montserrat_20,LV_PART_MAIN);    
                            } 
                            
                            char *ip_address = (char *)received_msg.data;
                            lv_label_set_text_fmt(ui_ip_label, "IP: %s", ip_address);
                            
                            // Release the memory allocated by the event handler.
                            free(ip_address);
                        break;


                        // 1. Initial SNTP synchronization successful.  
                        //2. After the initial synchronization is successful, the local time is considered correct, 
                        //and the corresponding controls update every second accordingly.
                        case UI_MSG_SNTP_SYNED:

                            time_t now;
                            struct tm timeinfo;

                            time(&now);

                            // Convert a timestamp to a time structure.
                            localtime_r(&now, &timeinfo);
                            char time_buff[16];

                            // Convert the current time from the time structure into a string formatted as %02d:%02d:%02d for easy display on the screen.
                            tm_to_string(&timeinfo,time_buff);

                            // Change the UI clock time
                            lv_label_set_text(ui_HourMinuteLabel,time_buff);

                            // Change the angle of the real-time clock
                            lv_img_set_angle(ui_second,60*(timeinfo.tm_sec));
                            lv_img_set_angle(ui_min, 60*(timeinfo.tm_min));
                            lv_img_set_angle(ui_hour,300*(timeinfo.tm_hour));



                        break;

                        case UI_MSG_WEATHER_RES:

                            weather_data_t *data=(weather_data_t *)received_msg.data;
                            
                            ui_weather_update(*data); 

                            // Release allocated heap memory
                            free(data);

                        break;

                        case UI_MSG_WIFI_DISCONNECTED:
                            if(received_msg.data !=NULL)
                            {
                                uint8_t *exception_disconnect_flag=(uint8_t *)(received_msg.data);
                                if(*exception_disconnect_flag ==1)
                                {
                                    if(ui_ip_label!=NULL)
                                    {
                                        lv_label_set_text(ui_ip_label, "wifi is disconnected,please refresh");
                                    }

                                }
                                
                                free(exception_disconnect_flag);

                            
                            }                        
                        break;

                        case UI_MSG_SD_REFRESH_RES:
                            file_refresh_res_data_t *res_data=(file_refresh_res_data_t *)(received_msg.data);
                            
                            if(res_data->err ==ESP_OK)
                            {

                                strcpy(sd_current_dir,res_data->current_path);
                                
                                size_t length=strlen(sd_current_dir);
                                // Due to screen width limitations, when the length of the sd_current_dir string is >= 24 bytes (excluding the '\0'), it is split into two strings for display.
                                if(length>=24)
                                {
                                    char temp_arr[32];
                                    strncpy(temp_arr, sd_current_dir, 24);
                                    temp_arr[24] = '\0'; // Ensure the string is null-terminated with '\0' at the end.

                                    // The remaining part (copy starting from the 24th character)
                                    strncpy(branch_sd_current_dir, sd_current_dir + 24, length-24);
                                    branch_sd_current_dir[length-24] = '\0';
                                
                                    lv_label_set_text(main_path_label,temp_arr);

                                    lv_label_set_text(branch_path_label,branch_sd_current_dir);

                                }
                                // Length less than 24 bytes
                                else
                                {
                                    lv_label_set_text(main_path_label,sd_current_dir);
                                    
                                    // Note the situation here: this path label
                                    memset(branch_sd_current_dir,'\0',32);
                                    lv_label_set_text(branch_path_label,branch_sd_current_dir);

                                }
                                lv_obj_clean(file_list);

                                // If there are no files or directories in the current directory
                                if(res_data->item_count ==0)
                                {
                                    lv_list_add_btn(file_list, LV_SYMBOL_CLOSE, "empty directory");
                                }
                                // Has content, iterate and display
                                else 
                                {
                                    //because limited ram ,so when the numbe of file too much
                                    if(res_data->item_count >=SD_REFRESH_FILE_COUNT_MAX)
                                    {
                                        res_data->item_count=SD_REFRESH_FILE_COUNT_MAX;
                                    }

                                    for (uint8_t  i = 0; i < res_data->item_count; i++) 
                                    {
                                        const char *icon = res_data->items[i].is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
                                        lv_obj_t *btn = lv_list_add_btn(file_list, icon, res_data->items[i].name);

                                        uint8_t *is_directory_flag=heap_caps_malloc(sizeof(char), MALLOC_CAP_SPIRAM| MALLOC_CAP_8BIT);
                                        *is_directory_flag= res_data->items[i].is_dir;
                                            
                                        lv_obj_add_event_cb(btn, file_list_click_cb, LV_EVENT_CLICKED, is_directory_flag);  
                                                                                    
                                    }
                                    
                                    free(res_data->items);


                                }
                            } else 
                            {
                                lv_label_set_text(main_path_label, "Error opening directory");
                            }
                        break;
                            
                        //pasword entry event 
                        case UI_MSG_BLE_PAIR_PASS_ENTRY:
                                if(ui_bt!=NULL)
                                {
                                    if(!ui_ble_pass_label)
                                    {
                                        ui_ble_pass_label=lv_label_create(ui_bt);
                                        lv_obj_set_size(ui_ble_pass_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                        lv_obj_clear_flag(ui_ble_pass_label, LV_OBJ_FLAG_HIDDEN);      // Flags
                                        lv_obj_align(ui_ble_pass_label,LV_ALIGN_CENTER,0,10);
                                    }

                                    int  pass=(int)(*((uint32_t *)received_msg.data));
                                    lv_label_set_text_fmt(ui_ble_pass_label,"pass: %06d", pass);
                                    lv_obj_clear_flag(ui_ble_pass_label, LV_OBJ_FLAG_HIDDEN);      // Flags
                                    
                                    free(received_msg.data);

                                }
                        break;
                            
                        //ble pair success 
                        case UI_MSG_BLE_PAIR_SUCCESS:
                                
                                //hide pass entry label
                                lv_obj_add_flag(ui_ble_pass_label, LV_OBJ_FLAG_HIDDEN);      // Flags

                                //g_btn_play_pause==NULL
                                if((!g_btn_play_pause) &&(ui_bt !=NULL))
                                {
                                    ui_bt_control_init();

                                }
                                //play/pause is not null,in this condition is hid host actively close the connection
                                else if(g_btn_play_pause)
                                {
                                    //only need to clear obj hiden flag
                                    ui_bt_control_display_all();

                                }
                        break;

                        //ble host actively close conenction
                        case UI_MSG_BLE_CONNECTION_CLOSE:

                                //in this condition is hid host actively close the connection
                                if(g_btn_play_pause && ui_bt)
                                {
                                    //only need to hide cousumer control button
                                    ui_bt_control_hide_all();
                                }

                        break; 

                        default: break;    
                }

                        
            // Unlock to ensure thread safety when calling LVGL API functions.
            lvgl_port_unlock();    
            }
            else
            {
                ESP_LOGE(TAG,"task lvgl mutex fail ");
            }
        }
    }
}
    



void tm_to_string(const struct tm *tm_data, char *buf)
{
    if (!tm_data || !buf) return;

    /* %02d means to pad with zeros if the number is less than two digits. */
    snprintf(buf, 9, "%02d:%02d:%02d",
             tm_data->tm_hour,
             tm_data->tm_min,
             tm_data->tm_sec);
}

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// Event handler: Converts ESP events into UI task messages and sends them, handler -----> message queue

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


// WIFI_EVENT_STA_START  handler
static void wifi_start_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) 
{
    ui_message_t msg;
    msg.type = UI_MSG_WIFI_START;
    ESP_LOGI(TAG, "Posting WIFI_START event to UI task queue.");
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
}


// WIFI_EVENT_STA_STOP  handler
static void wifi_stop_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) 
{
    ui_message_t msg;
    msg.type = UI_MSG_WIFI_STOP;
    ESP_LOGI(TAG, "Posting WIFI_STOP event to UI task queue.");
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
}


// WIFI_EVENT_SCAN_DONE  handler
static void wifi_scan_done_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    
    // The heap area pointed to by the event_data pointer will be reclaimed upon exiting the handler. Here, we manually copy it to prevent a wild pointer from occurring.
    wifi_scan_done_data_t  *cp_data=malloc(sizeof(wifi_scan_done_data_t));

    cp_data->ap_count=((wifi_scan_done_data_t *)event_data)->ap_count;
    cp_data->ap_records=((wifi_scan_done_data_t *)event_data)->ap_records;


    // Encapsulate Message
    ui_message_t msg;

    msg.type = UI_MSG_WIFI_SCAN_DONE;
    msg.data=(void *)cp_data;
    
    // Send message
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);

    ESP_LOGI(TAG, "Posting wifi scan done  event to UI task queue.");


}

// WIFI_GET_IP  handler
static void wifi_get_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    // Encapsulate the message and pass the data pointer.
    ui_message_t msg;
    msg.type = UI_MSG_WIFI_GET_IP;

    // Assuming event_data is a pointer to an IP address string, we make a copy of the data to ensure thread safety.
    char *ip_str = (char*)event_data;

    char *send_ip=malloc(strlen(ip_str)+1);

    strcpy(send_ip,ip_str);

    if (send_ip == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for IP string");
        return;
    }
    msg.data = send_ip;
    
    // Send message to UI task queue
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
    // ESP_LOGI(TAG, "Posting get IP event to UI task queue.");

}


//sntp syned handler
static void sntp_syned_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ui_message_t msg;
    msg.type = UI_MSG_SNTP_SYNED;

    // Send message to UI task queue
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);




}


static void weather_response_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ui_message_t msg;

    // Weather response data
    msg.type= UI_MSG_WEATHER_RES;

    // Since event_data is released after this handler exits, it is necessary to make a copy.
    weather_data_t  *cp_data= malloc( sizeof(weather_data_t));

    // Copy
    memcpy(cp_data,(weather_data_t* )event_data, sizeof(weather_data_t));

    msg.data=(void *)cp_data;

    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);




}




static void wifi_disconnected_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ui_message_t msg;
    msg.type=UI_MSG_WIFI_DISCONNECTED;
    
    // Abnormal disconnection scenarios (e.g., AP actively disconnecting / being out of range, etc.)
    if(event_data !=NULL)
    {
        uint8_t *cpdata=(uint8_t *)heap_caps_malloc(sizeof(uint8_t), MALLOC_CAP_SPIRAM |MALLOC_CAP_8BIT);
        *cpdata=*((uint8_t *)event_data);
        
        if(*cpdata ==1)
        {
            msg.data=(void *)cpdata;
            xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
        }
    }

    // Normal disconnection (incorrect password)
    else
    {
        msg.data=NULL;

        xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
    }


}


static void sd_refresh_response_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{

    //Since event_data will be released after this handler exits, 
    //a copy of it on heap needs to be made.
    file_refresh_res_data_t  *cp_data= heap_caps_malloc( sizeof(file_refresh_res_data_t), MALLOC_CAP_SPIRAM |MALLOC_CAP_8BIT);

    
    memcpy(cp_data,(file_refresh_res_data_t* )event_data, sizeof(file_refresh_res_data_t));

    ui_message_t msg;
    msg.type=UI_MSG_SD_REFRESH_RES;
    msg.data=(void *)cp_data;
    
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);

}


static void ble_pair_pass_entry_res_handler(void* arg, esp_event_base_t event_base, 
                                            int32_t event_id, void* event_data)
{
    //Since event_data will be released after this handler exits, 
    //a copy of it on heap needs to be made.
    uint32_t *pass_entry=(uint32_t *)malloc(sizeof(uint32_t ));
    *pass_entry=*((uint32_t *)event_data);

    ui_message_t msg;
    msg.type=UI_MSG_BLE_PAIR_PASS_ENTRY;
    msg.data=(void *)pass_entry;
    
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);





}

static void ble_pair_success_handler(void* arg, esp_event_base_t event_base, 
                        int32_t event_id, void* event_data)
{
    ui_message_t msg;
    msg.type=UI_MSG_BLE_PAIR_SUCCESS;
    msg.data=NULL;
    
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);





}

static void ble_connection_close_handler(void* arg, esp_event_base_t event_base, 
                        int32_t event_id, void* event_data)
{
    ui_message_t msg;
    msg.type=UI_MSG_BLE_CONNECTION_CLOSE;
    msg.data=NULL;
    
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);





}


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// Custom UI and adding event callback functions to UI widgets

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static void ap_button_click_cb(lv_event_t *e) 
{
    lv_obj_t *button = lv_event_get_target(e);  // Get the clicked button
    const char *ssid_text = lv_list_get_btn_text(wifi_list,button);  // Get button text (SSID)
    
    if (ssid_text != NULL) 
    {
        ESP_LOGI(TAG, "Clicked AP button with text: %s", ssid_text);
        strncpy(current_ssid, ssid_text, sizeof(current_ssid) - 1);  // Store SSID


    } else 
    {
        ESP_LOGE(TAG, "Failed to get button text");
    }


    ui_message_t msg = {.type = UI_MSG_WIFI_PASS_IN, .data = NULL};
    
    //transmit wifi_password_input event to notify ui_task to switch to uiPasswordScreen 
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
}


// Create password input page
void my_ui_create_password_screen(void) 
{
    ui_PasswordScreen = lv_obj_create(NULL);  
    lv_obj_set_style_bg_color(ui_PasswordScreen, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_PasswordScreen, 255, LV_PART_MAIN);

    // Create a password input box (textarea)
    ui_PasswordTextarea = lv_textarea_create(ui_PasswordScreen);
    lv_obj_set_size(ui_PasswordTextarea, LV_PCT(100), 180);  // Sample size
    lv_obj_align(ui_PasswordTextarea, LV_ALIGN_TOP_MID, 0, 0);  // Top Centered
    lv_textarea_set_placeholder_text(ui_PasswordTextarea, "Enter Password");
    lv_textarea_set_one_line(ui_PasswordTextarea, true);  // Single line
    lv_textarea_set_password_mode(ui_PasswordTextarea, false);  // Password mode (* displayed)
    
    // Create keyboard
    ui_Keyboard = lv_keyboard_create(ui_PasswordScreen);
    lv_obj_set_size(ui_Keyboard, LV_PCT(100), LV_PCT(60));  // Bottom half of the screen
    lv_obj_align(ui_Keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(ui_Keyboard, ui_PasswordTextarea);  // Bind to textarea
    
    // Create Connect button
    ui_ConnectButton = lv_btn_create(ui_PasswordScreen);
    lv_obj_set_size(ui_ConnectButton, 100, 40);
    lv_obj_align_to(ui_ConnectButton, ui_PasswordTextarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_add_event_cb(ui_ConnectButton, connect_button_click_cb, LV_EVENT_CLICKED, NULL);  // Register callback
    
    lv_obj_t *label = lv_label_create(ui_ConnectButton);
    lv_label_set_text(label, "Connect");
    lv_obj_center(label);
}



// Connect button click callback: send connection request event
static void connect_button_click_cb(lv_event_t *e) 
{
    const char *password = lv_textarea_get_text(ui_PasswordTextarea);
    if (password == NULL || strlen(password) == 0) 
    {
        ESP_LOGW(TAG, "Empty password, ignoring connect");
        return;  // Optional: Display error message
    }
    
    // Create event data
    wifi_connect_data_t data;
    strncpy(data.ssid, current_ssid, sizeof(data.ssid) - 1);
    strncpy(data.password, password, sizeof(data.password) - 1);
    
    // Send events to the UI event loop (subscribed by the WiFi component)
    esp_err_t ret = esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_WIFI_CONNECT_REQUEST, &data, sizeof(data), portMAX_DELAY);
    if (ret == ESP_OK) 
    {
        ui_message_t msg={.type=UI_MSG_WIFI_PASS_IN_DONE, .data=NULL};

        xQueueSend(ui_message_queue,&msg,portMAX_DELAY);

    } 
    else 
    {
        ESP_LOGE(TAG, "Failed to post connect request: %d", ret);
    }
}


// / @brief Update the weather on the weather screen
// / @param data The obtained weather data
static void ui_weather_update(weather_data_t data) 
{
    // Weather string
    lv_label_set_text(ui_TodayWeather, data.today_weather);

    // Address
    lv_label_set_text(ui_WeatherPosition, data.location);

    // Temperature
    lv_label_set_text(ui_Temperature, data.today_tem);

    // Set a three-day weather range
    lv_label_set_text(ui_day1Label, data._3day_tem_range[0]);
    lv_label_set_text(ui_day2Label, data._3day_tem_range[1]);
    lv_label_set_text(ui_day3Label, data._3day_tem_range[2]);
 
    ui_weather_change_img(data.today_weather_code, &ui_TodayWeaImage);

    ui_weather_change_img(data._3day_weather[0], &ui_day1Image);
    ui_weather_change_img(data._3day_weather[1], &ui_day2Image);
    ui_weather_change_img(data._3day_weather[2], &ui_day3Image);



}
// / @brief Change the corresponding image based on the weather code returned by the HTTP server. For details, please refer to https://seniverse.yuque.com/hyper_data/api_v3/yev2c3
/// @param weather_code     
// / @param obj  The address of the image widget object to be modified
static void ui_weather_change_img(int weather_code, lv_obj_t **obj)
{
    switch(weather_code)
    {
        // 0-3 are all sunny under the same conditions.
        case 0:
        case 1:
        case 2:
        case 3:
            lv_img_set_src(*obj,&ui_img_sunimage_png);

        break;

        case 4:
            lv_img_set_src(*obj,&ui_img_cloudyimage1_png);
        break;

        // 5-8: The same condition applies, all are Partly Cloudy.
        case 5:
        case 6:
        case 7:
        case 8:
            lv_img_set_src(*obj,&ui_img_partcloudyimage_png);
        break;
        
        case 9:
            lv_img_set_src(*obj,&ui_img_cloudyimage1_png);
        break;

        case 13:
            lv_img_set_src(*obj,&ui_img_lightrainimage_png);
        break;

        case 14:
            lv_img_set_src(*obj,&ui_img_moderaterainimage_png);
        break;

        // Lines 15-18 all represent the same situation: Heavy Rain.
        case 15:
        case 16:
        case 17:
        case 18:
            lv_img_set_src(*obj,&ui_img_heavyrainimage_png);
        break;

        default:
            break;

    }



}




