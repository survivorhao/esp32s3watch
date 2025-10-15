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
#include "event.h"
#include "wifi_connect.h"



//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static const char *TAG="my_ui";

static lv_obj_t *wifi_list = NULL;  // WiFi AP 列表 widget

static lv_obj_t *ui_PasswordScreen = NULL;  // 密码输入屏幕

static lv_obj_t *ui_PasswordTextarea = NULL;  // 密码输入框

static lv_obj_t *ui_Keyboard = NULL;  // 键盘

static lv_obj_t *ui_ConnectButton = NULL;  // Connect 按钮

static lv_obj_t  *ui_ip_label=NULL;

static lv_obj_t  *ui_ble_pass_label=NULL;

static char current_ssid[33] = {0};  // 临时存储点击的 SSID




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
    
    //wifi连接失败或者断开连接
    UI_MSG_WIFI_DISCONNECTED,     
    
    UI_MSG_SD_REFRESH_RES,
    
    UI_MSG_BLE_PAIR_PASS_ENTRY,

    UI_MSG_BLE_PAIR_SUCCESS,

    UI_MSG_BLE_CONNECTION_CLOSE,


} ui_message_type_t;


// 定义UI任务的消息结构体
typedef struct {
    ui_message_type_t type;
    void *data; // 使用一个通用指针来传递数据
} ui_message_t;

//ui task message queue
static QueueHandle_t ui_message_queue;


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

//将时间结构体当前时间转换为格式为 %02d:%02d:%02d字符串方便显示在display上面
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
    // 1. 创建消息队列
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
        // 阻塞等待消息
        if (xQueueReceive(ui_message_queue, &received_msg, portMAX_DELAY) == pdTRUE) 
        {
            // 加锁以安全地调用LVGL API，确保lvgl api thread safe 
            if (lvgl_port_lock(0) == true) 
            {
                
                switch (received_msg.type) 
                {
                    case UI_MSG_WIFI_START:
                        
                        
                        // 隐藏ui_wifi_off_image图片
                        lv_obj_add_flag(ui_WifiOffImage, LV_OBJ_FLAG_HIDDEN);      // Flags

                        // 显示ui_wifi_on_image图片
                        lv_obj_clear_flag(ui_WIfiOnImage, LV_OBJ_FLAG_HIDDEN);     /// Flags
                        break;
                    case UI_MSG_WIFI_STOP:
                        
                        //隐藏ui_wifi_on_image图片
                        lv_obj_add_flag(ui_WIfiOnImage, LV_OBJ_FLAG_HIDDEN);      // Flags

                        //显示ui_wifi_off_image图片
                        lv_obj_clear_flag(ui_WifiOffImage, LV_OBJ_FLAG_HIDDEN);     /// Flags

                        if(ui_ip_label!=NULL)
                        {
                            lv_obj_del_delayed(ui_ip_label,100);
                            ui_ip_label=NULL;
                        }

                        if(wifi_list!= NULL)
                        {
                            lv_obj_del_delayed(wifi_list,200);

                            //wifi_list置NULL，下次scan done重新创建
                            wifi_list=NULL;
                        }


                        break;
                    case UI_MSG_WIFI_SCAN_DONE:


                        ESP_LOGI(TAG, "Received scan results, updating UI list...");

                        wifi_scan_done_data_t *ap_list=(wifi_scan_done_data_t* )received_msg.data;
                        if (ap_list == NULL || ap_list->ap_count == 0) 
                        {
                            ESP_LOGW(TAG, "No APs found or invalid data");
                            // 可选：在 UI 显示 "No WiFi found" 标签
                            break;
                        }

                        // 创建或更新 list widget（假设 ui_WifiScreen 是 WiFi 屏幕对象）
                        if (wifi_list == NULL) 
                        {
                            
                            wifi_list = lv_list_create(ui_wifi);        // 创建在 WiFi screen 上

                            lv_obj_set_size(wifi_list, LV_PCT(100), LV_PCT(60));  // 示例大小：全宽，80% 高
                            
                            lv_obj_set_align(wifi_list, LV_ALIGN_CENTER);  // 居中
            
                            lv_obj_set_pos(wifi_list,0,55);

                            lv_obj_set_style_bg_color(wifi_list, lv_color_hex(0xffffff), LV_PART_MAIN);  
                            lv_obj_set_style_bg_opa(wifi_list, 255, LV_PART_MAIN);  
                            lv_obj_set_style_border_color(wifi_list, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_opa(wifi_list, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            
                        }else 
                        {
                            // 如果 list 已存在，清空旧内容
                            lv_obj_clean(wifi_list);
                        }

                        // 遍历 ap_records，添加按钮
                        for (uint8_t i = 0; i < ap_list->ap_count; i++) 
                        {
                            // 添加按钮：图标 LV_SYMBOL_WIFI + SSID
                            // lv_list_add_btn 返回 lv_obj_t * (按钮对象)
                            lv_obj_t *btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WIFI, (const char *)ap_list->ap_records[i].ssid);
                            
                            lv_obj_set_style_bg_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);  
                            lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN); 
                            
                            if (btn != NULL) 
                            {
                                // 注册点击事件
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

                        //成功get ip之后在删除wifi_list 以及password input screen 
                        case UI_MSG_WIFI_GET_IP:
                            
                            //lv_obj_del api
                            lv_obj_del_delayed(wifi_list,150);

                            //wifi_list置NULL，下次scan done重新创建
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
                            
                            // 释放由事件处理器分配的内存
                            free(ip_address);
                        break;


                        //1，初次sntp同步成功，2，在初次同步成功后，本地时间即为正确时间，每秒改变相应控件
                        case UI_MSG_SNTP_SYNED:

                            time_t now;
                            struct tm timeinfo;

                            time(&now);

                            //将时间戳转换为时间结构体
                            localtime_r(&now, &timeinfo);
                            char time_buff[16];

                            //将时间结构体当前时间转换为格式为 %02d:%02d:%02d字符串方便显示在display上面
                            tm_to_string(&timeinfo,time_buff);

                            //更改ui clock时间
                            lv_label_set_text(ui_HourMinuteLabel,time_buff);

                            //改变实时时钟角度
                            lv_img_set_angle(ui_second,60*(timeinfo.tm_sec));
                            lv_img_set_angle(ui_min, 60*(timeinfo.tm_min));
                            lv_img_set_angle(ui_hour,300*(timeinfo.tm_hour));



                        break;

                        case UI_MSG_WEATHER_RES:

                            weather_data_t *data=(weather_data_t *)received_msg.data;
                            
                            ui_weather_update(*data); 

                            //释放分配到heap
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
                                //由于屏幕宽度限制原因，当sd_current_dir字符串长度>=24 byte（不包含'\0'）,就把其拆为两个字符串显示
                                if(length>=24)
                                {
                                    char temp_arr[32];
                                    strncpy(temp_arr, sd_current_dir, 24);
                                    temp_arr[24] = '\0'; // 确保结尾有 '\0'

                                    // 剩余部分（从第 24 个字符开始复制）
                                    strncpy(branch_sd_current_dir, sd_current_dir + 24, length-24);
                                    branch_sd_current_dir[length-24] = '\0';
                                
                                    lv_label_set_text(main_path_label,temp_arr);

                                    lv_label_set_text(branch_path_label,branch_sd_current_dir);

                                }
                                //长度小于24 byte
                                else
                                {
                                    lv_label_set_text(main_path_label,sd_current_dir);
                                    
                                    //注意这里情况此路径label
                                    memset(branch_sd_current_dir,'\0',32);
                                    lv_label_set_text(branch_path_label,branch_sd_current_dir);

                                }
                                lv_obj_clean(file_list);

                                //如果说当前目录里无文件或者目录
                                if(res_data->item_count ==0)
                                {
                                    lv_list_add_btn(file_list, LV_SYMBOL_CLOSE, "empty directory");
                                }
                                //有内容，遍历显示
                                else 
                                {
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

                        
            // 解锁,确保lvgl api调用的thread safe 
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

    /* %02d 表示不足两位补零 */
    snprintf(buf, 9, "%02d:%02d:%02d",
             tm_data->tm_hour,
             tm_data->tm_min,
             tm_data->tm_sec);
}

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// 事件处理器：将ESP事件转换为UI任务消息并发送, handler -----> message queue

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
    
    //  event_data指针指向的heap区域，在退出handler会被回收，这里手动拷贝一份，防止出现wild pointer
    wifi_scan_done_data_t  *cp_data=malloc(sizeof(wifi_scan_done_data_t));

    cp_data->ap_count=((wifi_scan_done_data_t *)event_data)->ap_count;
    cp_data->ap_records=((wifi_scan_done_data_t *)event_data)->ap_records;


    // 封装消息
    ui_message_t msg;

    msg.type = UI_MSG_WIFI_SCAN_DONE;
    msg.data=(void *)cp_data;
    
    // 发送消息
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);

    ESP_LOGI(TAG, "Posting wifi scan done  event to UI task queue.");


}

// WIFI_GET_IP  handler
static void wifi_get_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    // 封装消息，传递数据指针
    ui_message_t msg;
    msg.type = UI_MSG_WIFI_GET_IP;

    // 假设event_data是一个指向ip地址字符串的指针，我们拷贝一份数据以确保线程安全
    char *ip_str = (char*)event_data;

    char *send_ip=malloc(strlen(ip_str)+1);

    strcpy(send_ip,ip_str);

    if (send_ip == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for IP string");
        return;
    }
    msg.data = send_ip;
    
    // 发送消息到UI任务队列
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
    // ESP_LOGI(TAG, "Posting get IP event to UI task queue.");

}


//sntp syned handler
static void sntp_syned_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ui_message_t msg;
    msg.type = UI_MSG_SNTP_SYNED;

    // 发送消息到UI任务队列
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);




}


static void weather_response_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ui_message_t msg;

    //天气响应数据
    msg.type= UI_MSG_WEATHER_RES;

    //由于evnet_data 在此handler退出后被释放，因此需要拷贝一份
    weather_data_t  *cp_data= malloc( sizeof(weather_data_t));

    //拷贝
    memcpy(cp_data,(weather_data_t* )event_data, sizeof(weather_data_t));

    msg.data=(void *)cp_data;

    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);




}




static void wifi_disconnected_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ui_message_t msg;
    msg.type=UI_MSG_WIFI_DISCONNECTED;
    
    //异常断开连接情况（ap主动关闭/距离太远等事件）
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

    //正常断开连接（密码错误）
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

// 自定义Ui 以及给ui widget添加的事件回调函数

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static void ap_button_click_cb(lv_event_t *e) 
{
    lv_obj_t *button = lv_event_get_target(e);  // 获取被点击的按钮
    const char *ssid_text = lv_list_get_btn_text(wifi_list,button);  // 获取按钮文本（SSID）
    
    if (ssid_text != NULL) 
    {
        ESP_LOGI(TAG, "Clicked AP button with text: %s", ssid_text);
        strncpy(current_ssid, ssid_text, sizeof(current_ssid) - 1);  // 存储 SSID


    } else 
    {
        ESP_LOGE(TAG, "Failed to get button text");
    }


    ui_message_t msg = {.type = UI_MSG_WIFI_PASS_IN, .data = NULL};
    
    //transmit wifi_password_input event to notify ui_task to switch to uiPasswordScreen 
    xQueueSend(ui_message_queue, &msg, portMAX_DELAY);
}


// 创建密码输入页面
void my_ui_create_password_screen(void) 
{
    ui_PasswordScreen = lv_obj_create(NULL);  
    lv_obj_set_style_bg_color(ui_PasswordScreen, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_PasswordScreen, 255, LV_PART_MAIN);

    // 创建密码输入框 (textarea)
    ui_PasswordTextarea = lv_textarea_create(ui_PasswordScreen);
    lv_obj_set_size(ui_PasswordTextarea, LV_PCT(100), 180);  // 示例大小
    lv_obj_align(ui_PasswordTextarea, LV_ALIGN_TOP_MID, 0, 0);  // 上方居中
    lv_textarea_set_placeholder_text(ui_PasswordTextarea, "Enter Password");
    lv_textarea_set_one_line(ui_PasswordTextarea, true);  // 单行
    lv_textarea_set_password_mode(ui_PasswordTextarea, false);  // 密码模式（* 显示）
    
    // 创建键盘
    ui_Keyboard = lv_keyboard_create(ui_PasswordScreen);
    lv_obj_set_size(ui_Keyboard, LV_PCT(100), LV_PCT(60));  // 下半屏
    lv_obj_align(ui_Keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(ui_Keyboard, ui_PasswordTextarea);  // 绑定到 textarea
    
    // 创建 Connect 按钮
    ui_ConnectButton = lv_btn_create(ui_PasswordScreen);
    lv_obj_set_size(ui_ConnectButton, 100, 40);
    lv_obj_align_to(ui_ConnectButton, ui_PasswordTextarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_add_event_cb(ui_ConnectButton, connect_button_click_cb, LV_EVENT_CLICKED, NULL);  // 注册回调
    
    lv_obj_t *label = lv_label_create(ui_ConnectButton);
    lv_label_set_text(label, "Connect");
    lv_obj_center(label);
}



// Connect 按钮点击回调：发送连接请求事件
static void connect_button_click_cb(lv_event_t *e) 
{
    const char *password = lv_textarea_get_text(ui_PasswordTextarea);
    if (password == NULL || strlen(password) == 0) 
    {
        ESP_LOGW(TAG, "Empty password, ignoring connect");
        return;  // 可选：显示错误提示
    }
    
    // 创建事件数据
    wifi_connect_data_t data;
    strncpy(data.ssid, current_ssid, sizeof(data.ssid) - 1);
    strncpy(data.password, password, sizeof(data.password) - 1);
    
    // 发送事件到 UI event loop（WiFi 组件会订阅）
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


/// @brief 更新weather screen的天气
/// @param data 获得的天气数据
static void ui_weather_update(weather_data_t data) 
{
    //天气字符串
    lv_label_set_text(ui_TodayWeather, data.today_weather);

    //地址
    lv_label_set_text(ui_WeatherPosition, data.location);

    //温度
    lv_label_set_text(ui_Temperature, data.today_tem);

    //设置三天天气范围
    lv_label_set_text(ui_day1Label, data._3day_tem_range[0]);
    lv_label_set_text(ui_day2Label, data._3day_tem_range[1]);
    lv_label_set_text(ui_day3Label, data._3day_tem_range[2]);
 
    ui_weather_change_img(data.today_weather_code, &ui_TodayWeaImage);

    ui_weather_change_img(data._3day_weather[0], &ui_day1Image);
    ui_weather_change_img(data._3day_weather[1], &ui_day2Image);
    ui_weather_change_img(data._3day_weather[2], &ui_day3Image);



}
/// @brief 根据http server返回的weather code改变相应的图片,具体请参考https://seniverse.yuque.com/hyper_data/api_v3/yev2c3
/// @param weather_code     
/// @param obj  要改变的image wideget 对象的地址
static void ui_weather_change_img(int weather_code, lv_obj_t **obj)
{
    switch(weather_code)
    {
        //0-3同一情况均为sunny
        case 0:
        case 1:
        case 2:
        case 3:
            lv_img_set_src(*obj,&ui_img_sunimage_png);

        break;

        case 4:
            lv_img_set_src(*obj,&ui_img_cloudyimage1_png);
        break;

        //5-8同一情况均为Partly Cloudy
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

        //15-18同一情况均为Heavy Rain
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




