#include "../ui.h"

lv_obj_t * ui_calender=NULL;
lv_obj_t * calendar=NULL;
lv_obj_t * calender_exit_but=NULL;

void ui_calender_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) 
    {

        _ui_screen_change(&ui_app2, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_app2_screen_init);     
        
        if(ui_calender!=NULL)
        {
            lv_obj_del_delayed(ui_calender,300);

            ui_calender=NULL;
            calendar=NULL;
            calender_exit_but=NULL;

        }
    }
}


void ui_calender_screen_init(void)
{

    //screen 创建 
    ui_calender= lv_obj_create(NULL);  
    lv_obj_clear_flag(ui_calender, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
    LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    
    lv_obj_set_style_bg_color(ui_calender, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_calender, 255, LV_PART_MAIN);

    calendar = lv_calendar_create(ui_calender);
    lv_obj_set_size(calendar, lv_pct(100), lv_pct(80));
    lv_obj_align(calendar,LV_ALIGN_CENTER,0,10);

    lv_calendar_set_today_date(calendar, 2025, 10, 9);
    lv_calendar_set_showed_date(calendar, 2025, 10);
    static const char * day_names[7] = {"Sun", "Mon", "Tue", "Wed", "Thur", "Fri", "Sat"};
    lv_calendar_set_day_names(calendar, day_names);
    lv_calendar_header_dropdown_create(calendar);
    lv_calendar_header_arrow_create(calendar);


    calender_exit_but = lv_img_create(ui_calender);
    lv_img_set_src(calender_exit_but , &ui_img_return_png);
    lv_obj_set_width(calender_exit_but , LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(calender_exit_but , LV_SIZE_CONTENT);    /// 1
    lv_obj_align(calender_exit_but , LV_ALIGN_TOP_MID, -90, -80);
    lv_obj_add_flag(calender_exit_but , LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(calender_exit_but , LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                        LV_OBJ_FLAG_SNAPPABLE  | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                        LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(calender_exit_but , 50);
    lv_obj_set_style_bg_color(calender_exit_but , lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(calender_exit_but , 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(calender_exit_but , lv_color_hex(0x000000), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(calender_exit_but , 0, LV_PART_MAIN);

    lv_obj_add_event_cb(calender_exit_but , ui_calender_return_event_cb, LV_EVENT_CLICKED, NULL);    

    // esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_CAMERA_ENTER, NULL, 0, portMAX_DELAY);

}