#include "../ui.h"
#include <string.h>
#include "weather_srv.h"
#include "bsp_driver.h"

lv_obj_t *ui_setting=NULL;
lv_obj_t *setting_exit_but=NULL;
lv_obj_t *setting_list=NULL;


//now be clicked button in setting list
lv_obj_t *current_click_button=NULL;


//weather position relevant widget
lv_obj_t *weather_pos=NULL;
lv_obj_t *wea_pos_text_area=NULL;
lv_obj_t *wea_pos_keyboard=NULL;
lv_obj_t *wea_pos_ensure_but=NULL;


lv_obj_t *lightness_screen=NULL;
lv_obj_t *lightness_slider=NULL;
lv_obj_t *slider_label=NULL;
lv_obj_t *lightness_exit_but=NULL;

static const char *TAG="ui_setting ";

//total setting options number is 
#define TOTAL_SETTING_OPTIONS    3




//---------------------------------------------------------------------------------------------
//---------------------------------------start---------------------------------------------------
void weather_pos_handler(void *arg);
void lightness_adjust_handler(void *arg);


//---------------------------------------end---------------------------------------------------
//---------------------------------------------------------------------------------------------





void save_custom_setting_to_nvs(void);
void append_city(char *arr, size_t arr_size, const char *city);
void nvs_store_weather_pos_load(void);

//setting array,which will be argument as lv_list_add_btn
static char   Setting_option_array[10][64]={
                    "screen ligntness ", 
                    "weather position ",
                    "author:survivorhao "
                    "",
                    "",

                    "",
                    "",
                    "",
                    "",
                    "",
                    
};

//each setting option coule have a handler which can be registered here, type is void (*handler)(void *) 
static const void (*Setting_option_handler[10])(void *)={
                    lightness_adjust_handler,
                    weather_pos_handler,
                    NULL,
                    NULL,
                    NULL,

                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
};



/// @brief click this button to return to previous screen 
/// @param e 
void ui_setting_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) 
    {

        _ui_screen_change(&ui_app1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_app1_screen_init);     
        
        if(ui_setting!=NULL)
        {
            lv_obj_del_delayed(ui_setting,300);
            ui_setting=NULL;
            setting_exit_but=NULL;

        }
        if(weather_pos!=NULL)
        {
            lv_obj_del_delayed(weather_pos,500);
            weather_pos=NULL;
        }
        if(lightness_screen!=NULL)
        {
            lv_obj_del_delayed(lightness_screen,700);
            lightness_screen=NULL;
        }

        save_custom_setting_to_nvs();


    }
}


/// @brief 
/// @param e 
static void ensure_button_click_cb(lv_event_t *e) 
{
    const char *text = lv_textarea_get_text(wea_pos_text_area);
    if (text == NULL || strlen(text) == 0) 
    {
        ESP_LOGW(TAG, "NULL input, ignoring change weather position request");
        return;  // 可选：显示错误提示
    }
    
    ESP_LOGI(TAG,"change weather pos to %s", text);

    //copy current weather position
    strncpy(weather_position, text, sizeof(weather_position));
    
    //post  APP_WEATHER_POSITION_CHANGE
    esp_err_t ret = esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_WEATHER_POSITION_CHANGE, NULL, 0, portMAX_DELAY);
    if(ret == ESP_OK) 
    {
        append_city(Setting_option_array[1], sizeof(Setting_option_array[1]), weather_position);
        
        ESP_LOGI(TAG,"now complete is %s",Setting_option_array[1]);
        for(uint8_t i = 0; i < lv_obj_get_child_cnt(current_click_button); i++) 
        {
            lv_obj_t * child = lv_obj_get_child(current_click_button, i);
            if(lv_obj_check_type(child, &lv_label_class)) 
            {
                
                lv_label_set_text(child, Setting_option_array[1]);
                break;
            }
        }

        _ui_screen_change(&ui_setting, LV_SCR_LOAD_ANIM_NONE, 0, 100, ui_setting_screen_init);  

    } 
    else 
    {
        ESP_LOGE(TAG, "Failed to post connect request: %d", ret);
    }

}

/// @brief 
/// @param e 
static void setting_button_click_cb(lv_event_t *e) 
{
    current_click_button = lv_event_get_target(e);  // 获取被点击的按钮
    const char *option = lv_list_get_btn_text(setting_list,current_click_button);  // 获取按钮文本（SSID）
    
    if (option != NULL) 
    {
        ESP_LOGI(TAG, "setting %s ....", option);
        
        uint8_t option_index=0;

        for(option_index=0; option_index< TOTAL_SETTING_OPTIONS; option_index++)
        {
            //find clicked setting option's index 
            if( !(strcmp(option, Setting_option_array[option_index])))
            {
                break;
            }
        }

        //when handler is not null 
        if(Setting_option_handler[option_index] )
        {
            //find setting option associate index then call relevant handler
            Setting_option_handler[option_index](NULL);
        }

    } else 
    {
        ESP_LOGE(TAG, "Failed to get button text");
    }


}

static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    char buf[8];
    int value=(int)lv_slider_get_value(slider);

    lv_snprintf(buf, sizeof(buf), "%d%%", value);
    lv_label_set_text(slider_label, buf);
    
    //adjust bightness
    bsp_display_brightness_set(value);


}


/// @brief click this button to return to previous screen 
/// @param e 
void ui_lightness_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) 
    {

        _ui_screen_change(&ui_setting, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_setting_screen_init);     
        
    }
}




/// @brief 
/// @param  
void ui_setting_screen_init(void )
{
    //create screen 
    ui_setting= lv_obj_create(NULL);  
    lv_obj_clear_flag(ui_setting, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
    LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    
    lv_obj_set_style_bg_color(ui_setting, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_setting, 255, LV_PART_MAIN);

    //create exit button (which be pressed will change current display to before one )
    setting_exit_but = lv_img_create(ui_setting);
    lv_img_set_src(setting_exit_but , &ui_img_return_png);
    lv_obj_set_width(setting_exit_but , LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(setting_exit_but , LV_SIZE_CONTENT);    /// 1
    lv_obj_align(setting_exit_but , LV_ALIGN_TOP_MID, -90, -80);
    lv_obj_add_flag(setting_exit_but , LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(setting_exit_but , LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                        LV_OBJ_FLAG_SNAPPABLE  | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                        LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(setting_exit_but , 50);
    lv_obj_set_style_bg_color(setting_exit_but , lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(setting_exit_but , 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(setting_exit_but , lv_color_hex(0x000000), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(setting_exit_but , 0, LV_PART_MAIN);

    
    setting_list = lv_list_create(ui_setting);        

    lv_obj_set_size(setting_list, LV_PCT(100), LV_PCT(90));  // 示例大小：全宽，80% 高
    lv_obj_set_align(setting_list, LV_ALIGN_CENTER);  // 居中
    lv_obj_set_pos(setting_list,0,18);
    lv_obj_set_style_bg_color(setting_list, lv_color_hex(0xffffff), LV_PART_MAIN);  
    lv_obj_set_style_bg_opa(setting_list, 255, LV_PART_MAIN);  
    lv_obj_set_style_border_color(setting_list, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(setting_list, 255, LV_PART_MAIN | LV_STATE_DEFAULT);    

    lv_obj_t *list_button=NULL;


    //append nvs weather position data to specific Setting option
    append_city(Setting_option_array[1], sizeof(Setting_option_array[1]), weather_position);

    //create setting options
    for(uint8_t i=0; i<TOTAL_SETTING_OPTIONS; i++)
    {
        
        list_button= lv_list_add_btn(setting_list, LV_SYMBOL_SETTINGS, Setting_option_array[i]); 
        if(list_button!=NULL)
        {
            lv_obj_add_event_cb(list_button, setting_button_click_cb, LV_EVENT_CLICKED, NULL);
        }

    }
  
    //add return button call back function
    lv_obj_add_event_cb(setting_exit_but , ui_setting_return_event_cb, LV_EVENT_CLICKED, NULL);    








}


//weather position setting input 

void ui_weather_pos_screen_init(void) 
{
    weather_pos = lv_obj_create(NULL);  
    lv_obj_set_style_bg_color(weather_pos, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(weather_pos, 255, LV_PART_MAIN);
    lv_obj_clear_flag(weather_pos, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
    LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    // 创建密码输入框 (textarea)
    wea_pos_text_area = lv_textarea_create(weather_pos);
    lv_obj_set_size(wea_pos_text_area, LV_PCT(100), 180);  // 示例大小
    lv_obj_align(wea_pos_text_area, LV_ALIGN_TOP_MID, 0, 0);  // 上方居中
    lv_textarea_set_placeholder_text(wea_pos_text_area, "Enter current weather position");
    lv_textarea_set_one_line(wea_pos_text_area, true);  // 单行
    lv_textarea_set_password_mode(wea_pos_text_area, false);  // 密码模式（* 显示）
    
    // 创建键盘
    wea_pos_keyboard = lv_keyboard_create(weather_pos);
    lv_obj_set_size(wea_pos_keyboard, LV_PCT(100), LV_PCT(60));  // 下半屏
    lv_obj_align(wea_pos_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(wea_pos_keyboard, wea_pos_text_area);  // 绑定到 textarea
    
    // 创建 Connect 按钮
    wea_pos_ensure_but = lv_btn_create(weather_pos);
    lv_obj_set_size(wea_pos_ensure_but, 100, 40);
    lv_obj_align_to(wea_pos_ensure_but, wea_pos_text_area, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_add_event_cb(wea_pos_ensure_but, ensure_button_click_cb, LV_EVENT_CLICKED, NULL);  // 注册回调
    
    lv_obj_t *label = lv_label_create(wea_pos_ensure_but);
    lv_label_set_text(label, "ensure");
    lv_obj_center(label);
}

//screen lightness adjust

void ui_lightness_screen_init(void) 
{
    lightness_screen = lv_obj_create(NULL);  
    lv_obj_set_style_bg_color(lightness_screen, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lightness_screen, 255, LV_PART_MAIN);
    lv_obj_clear_flag(lightness_screen, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
    LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    //create exit button (which be pressed will change current display to before one )
    lightness_exit_but = lv_img_create(lightness_screen);
    lv_img_set_src(lightness_exit_but , &ui_img_return_png);
    lv_obj_set_width(lightness_exit_but , LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(lightness_exit_but , LV_SIZE_CONTENT);    /// 1
    lv_obj_align(lightness_exit_but , LV_ALIGN_TOP_MID, -90, -80);
    lv_obj_add_flag(lightness_exit_but , LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(lightness_exit_but , LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                        LV_OBJ_FLAG_SNAPPABLE  | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                        LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(lightness_exit_but , 50);
    lv_obj_set_style_bg_color(lightness_exit_but , lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lightness_exit_but , 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(lightness_exit_but , lv_color_hex(0x000000), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(lightness_exit_but , 0, LV_PART_MAIN);
    lv_obj_add_event_cb(lightness_exit_but , ui_lightness_return_event_cb, LV_EVENT_CLICKED, NULL); 


    /*Create a lightness_slider in the center of the display*/
    lightness_slider = lv_slider_create(lightness_screen);
    lv_obj_set_size(lightness_slider,LV_PCT(85),LV_PCT(5));
    lv_obj_center(lightness_slider);
    lv_obj_add_event_cb(lightness_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_anim_time(lightness_slider, 200, 0);
    

    /*Create a label below the lightness_slider*/
    slider_label = lv_label_create(lightness_screen);
    lv_label_set_text(slider_label, "0%%");

    lv_obj_align_to(slider_label, lightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    

}
void weather_pos_handler(void *arg)
{
    _ui_screen_change(&weather_pos, LV_SCR_LOAD_ANIM_NONE, 300, 0, ui_weather_pos_screen_init);
}


void lightness_adjust_handler(void *arg)
{
    _ui_screen_change(&lightness_screen, LV_SCR_LOAD_ANIM_NONE, 300, 0, ui_lightness_screen_init);


}
/// @brief save intput weather position to nvs
/// @param  
void save_custom_setting_to_nvs(void)
{
    //
    nvs_handle_t handle;
    esp_err_t err = nvs_open("user", NVS_READWRITE, &handle);
    if (err != ESP_OK) 
    {
       ESP_LOGE(TAG,"nvs open namespace fail ");
       return ;
    }
    nvs_set_str(handle, "weather_pos", weather_position);
    nvs_commit(handle);
    nvs_close(handle);




}


/// @brief 
/// @param arr 
/// @param arr_size 
/// @param city 
void append_city(char *arr, size_t arr_size, const char *city)
{
    const char *prefix = "weather position ";
    size_t prefix_len = strlen(prefix);
    size_t city_len = strlen(city);

    // 1. 确保数组够大
    if (prefix_len + city_len + 1 > arr_size) {
        printf("Error: buffer too small!\n");
        return;
    }

    // 2. 每次都从头重新写入前缀
    strcpy(arr, prefix);

    // 3. 拼接城市名
    strcat(arr, city);
}

