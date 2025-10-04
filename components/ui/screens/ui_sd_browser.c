#include "../ui.h"

//sd_browser screen屏幕
lv_obj_t* ui_sd_browser=NULL;
lv_obj_t* sd_browser_ret_but=NULL;

//当前路径
lv_obj_t* main_path_label=NULL;

//当前路径
lv_obj_t* branch_path_label=NULL;

//当前路径下的list
lv_obj_t* file_list=NULL;

//图片显示屏幕
lv_obj_t* ui_image_display=NULL;
lv_obj_t* image_display_ret_but=NULL;
lv_obj_t* image_display=NULL;

char sd_current_dir[SD_PATH_MAX]="/sdcard/";

//由于屏幕宽度限制原因，当sd_current_dir字符串长度>=24 byte（不包含'\0'）,就把其拆为两个字符串显示
//存放此字符串
char branch_sd_current_dir[SD_PATH_MAX]="";

static const char *TAG="ui_sd_browser";


void ui_sd_browser_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) 
    {
        // 当前为路径为/sdcard/，点击返回Button直接返回app screen并删除相关Ui
        if(strcmp(sd_current_dir, "/sdcard/")==0)
        {
            _ui_screen_change(&ui_app1, LV_SCR_LOAD_ANIM_NONE, 150, 0, NULL);
        
            if(ui_sd_browser !=NULL)
            {
                lv_obj_del_delayed(ui_sd_browser,250);
                ui_sd_browser=NULL;
                memset(sd_current_dir,0,SD_PATH_MAX);
                strcpy(sd_current_dir,"/sdcard/");

            }
            
            //删除图片显示screen
            if(ui_image_display !=NULL)
            {
                lv_obj_del_delayed(ui_image_display,350);
                ui_image_display=NULL;

            }
        }
        // 不为/sdcard/情况下点击返回，返回上一级目录
        else 
        {
            uint8_t  len = strlen(sd_current_dir);
            if (len > 0 && sd_current_dir[len - 1] == '/') 
            {
                sd_current_dir[len - 1] = '\0';
            }
            // 现在路径无尾部 '/', 找到最后一个 '/'
            char *last_slash = strrchr(sd_current_dir, '/');
            // 修改条件为 >= 以处理第一级子目录
            size_t root_len = strlen("/sdcard/");
            if (last_slash && last_slash >= sd_current_dir + root_len - 1) {
                *last_slash = '\0';
            }
            // 回退后添加尾部 '/' 以保持格式一致
            strcat(sd_current_dir, "/");

            file_refresh_req_data_t  data;
    
            data.is_directory=1;
            
            //由于是返回上级目录，没有具体点击哪个目录，name字段设置为空
            strcpy(data.name, "");
            strcpy(data.current_path, sd_current_dir);
            esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_FILE_REFRESH_REQUEST, &data, sizeof(data), portMAX_DELAY);
        }        
        
    }
}

/// @brief 点击文件列表事件
/// @param e 
void file_list_click_cb(lv_event_t *e) 
{
    lv_obj_t *button = lv_event_get_target(e);  // 获取被点击的按钮
    const char *name = lv_list_get_btn_text(file_list,button);  // 获取按钮文本（SSID）
    
    uint8_t *is_directory=(uint8_t *)lv_event_get_user_data(e);

    //请求刷新数据
    file_refresh_req_data_t  data;

    //点击目录
    if(is_directory&& (*is_directory)==1)
    {
        if(name != NULL) 
        {
            ESP_LOGI(TAG, "change to directory %s\n", name);
       
        }else 
        {
            ESP_LOGW(TAG,"name is empty! \n");
        }
    }
    //点击文件
    else if(is_directory&& (*is_directory)==0)
    {
        if(name != NULL) 
        {
            ESP_LOGI(TAG, "open file  %s\n", name);
       
        }else 
        {
            ESP_LOGW(TAG,"name is empty! \n");
        }


        //找到最后一个 '.'所在的字符串
        char *dot = strrchr(name, '.');
        //拷贝文件拓展名
        if (dot) 
        {
            strcpy(data.ext_name, dot + 1);
        } 
        //拓展名为空情况
        else 
        {
            data.ext_name[0] = '\0';
        }
    }
    data.is_directory=(*is_directory);
    strcpy(data.name, name);
    strcpy(data.current_path,sd_current_dir);
    

    esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_FILE_REFRESH_REQUEST, &data, sizeof(data), portMAX_DELAY);

    // free(is_directory);

}


void ui_image_display_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) 
    {
            // 当前为路径为/sdcard/，点击返回Button直接返回app screen并删除相关Ui
            _ui_screen_change(&ui_sd_browser, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, ui_sd_browser_screen_init);
            
    }



}

void ui_image_display_return(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) 
    {
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_sd_browser, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, ui_sd_browser_screen_init);

    }
}




/// @brief 
/// @param  
void ui_sd_browser_screen_init(void)
{
        //创建画布屏幕
    ui_sd_browser = lv_obj_create(NULL);  
    lv_obj_clear_flag(ui_sd_browser, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
    LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    
    lv_obj_set_style_bg_color(ui_sd_browser, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_sd_browser, 255, LV_PART_MAIN);

    sd_browser_ret_but = lv_img_create(ui_sd_browser);
    lv_img_set_src(sd_browser_ret_but, &ui_img_return_png);
    lv_obj_set_width(sd_browser_ret_but, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(sd_browser_ret_but, LV_SIZE_CONTENT);    /// 1
    lv_obj_align(sd_browser_ret_but, LV_ALIGN_TOP_MID, -90, -80);
    lv_obj_add_flag(sd_browser_ret_but, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(sd_browser_ret_but, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(sd_browser_ret_but, 50);
    lv_obj_set_style_bg_color(sd_browser_ret_but, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(sd_browser_ret_but, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_set_style_img_recolor(sd_browser_ret_but, lv_color_hex(0x000000), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(sd_browser_ret_but, 0, LV_PART_MAIN);


    main_path_label = lv_label_create(ui_sd_browser);
    lv_label_set_text_fmt(main_path_label, "Path:%s", sd_current_dir);
    lv_obj_set_size(main_path_label,LV_PCT(100),25);
    lv_obj_align(main_path_label, LV_ALIGN_TOP_MID, 0, 45);

    lv_obj_clear_flag(main_path_label, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_obj_set_style_text_font(main_path_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);


    //分支
    branch_path_label = lv_label_create(ui_sd_browser);

    //24
    lv_label_set_text(branch_path_label,branch_sd_current_dir);
    lv_obj_set_size(branch_path_label,LV_PCT(100),25);
    lv_obj_align(branch_path_label, LV_ALIGN_TOP_MID, 0, 65);

    lv_obj_clear_flag(branch_path_label, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_obj_set_style_text_font(branch_path_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);



    // 文件列表
    file_list = lv_list_create(ui_sd_browser);
    lv_obj_set_style_bg_color(file_list, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_set_size(file_list, LV_PCT(100), 200);
    lv_obj_align(file_list, LV_ALIGN_CENTER, 0, 40);  // 调整位置以留空间

    lv_obj_clear_flag(file_list, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                      LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags

    lv_obj_t *button= lv_list_add_btn(file_list, LV_SYMBOL_FILE, "pic");
    uint8_t *i=malloc(sizeof(uint8_t));
    *i=1;
    lv_obj_add_event_cb(button, file_list_click_cb, LV_EVENT_CLICKED,(void *)i);

    lv_obj_add_event_cb(sd_browser_ret_but, ui_sd_browser_return_event_cb, LV_EVENT_CLICKED, NULL);

    ui_image_display_screen_init();

}
/// @brief 
/// @param 
void ui_image_display_screen_init(void)
{
    
    ui_image_display = lv_obj_create(NULL);  
    lv_obj_clear_flag(ui_image_display, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
    LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    
    lv_obj_set_style_bg_color(ui_image_display, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_image_display, 255, LV_PART_MAIN);
 
        // 图片 widget
    image_display = lv_img_create(ui_image_display);
    lv_img_set_src(image_display, NULL);  // 设置源为 SD 卡路径（确保 LVGL FS 已配置）
    lv_obj_align(image_display, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_set_size(image_display,LV_PCT(100),LV_PCT(100));

    image_display_ret_but = lv_img_create(image_display);
    lv_img_set_src(image_display_ret_but, &ui_img_return_png);
    lv_obj_set_width(image_display_ret_but, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(image_display_ret_but, LV_SIZE_CONTENT);    /// 1
    lv_obj_align(image_display_ret_but, LV_ALIGN_TOP_MID, -90, -80);
    lv_obj_add_flag(image_display_ret_but, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(image_display_ret_but, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE  | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(image_display_ret_but, 50);
    lv_obj_set_style_bg_color(image_display_ret_but, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(image_display_ret_but, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_set_style_img_recolor(image_display_ret_but, lv_color_hex(0x000000), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(image_display_ret_but, 0, LV_PART_MAIN);


    

    

    // // 图片 widget
    // image_display = lv_img_create(ui_image_display);
    // lv_img_set_src(image_display, NULL);  // 设置源为 SD 卡路径（确保 LVGL FS 已配置）
    // lv_obj_align(image_display, LV_ALIGN_CENTER, 0, 0);
    
    // lv_obj_set_size(image_display,LV_PCT(100),LV_PCT(100));

    lv_obj_add_event_cb(image_display_ret_but, ui_image_display_return_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(image_display, ui_image_display_return, LV_EVENT_GESTURE, NULL);


}



