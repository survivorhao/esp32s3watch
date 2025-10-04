#include "../ui.h"

lv_obj_t* drawing_canvas=NULL;
lv_obj_t* ui_canvas=NULL;

lv_obj_t* canvas_screen_return_but=NULL;
lv_obj_t* canvas_screen_clean_but=NULL;
lv_obj_t* canvas_screen_clean_label=NULL;

lv_color_t* canvas_cbuf=NULL;

static void draw_event_cb(lv_event_t* e);

static void clear_button_event_cb(lv_event_t* e)
{
    // 获取事件触发的对象（按钮）
    lv_obj_t* btn = lv_event_get_target(e);

    // 调用 LVGL 画布 API 清空画布
    // 使用 lv_color_white() 填充为白色，LV_OPA_COVER 确保不透明
    lv_canvas_fill_bg(drawing_canvas, lv_color_hex(0x000000), LV_OPA_COVER);

}

void ui_canvas_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(&ui_app1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 150, 0, NULL);

        if(ui_canvas !=NULL)
        {
            lv_obj_del_delayed(ui_canvas,230);

            ui_canvas=NULL;
            drawing_canvas=NULL;
            canvas_screen_return_but=NULL;
            canvas_screen_clean_but=NULL;
            canvas_screen_clean_label=NULL;

            free(canvas_cbuf);
            canvas_cbuf=NULL;

        }
        
        
    }
}


void ui_canvas_screen_init(void)
{
    

    // 在 PSRAM 中动态分配画布缓冲区
    // MALLOC_CAP_SPIRAM 标志确保分配在外部 PSRAM 上
    canvas_cbuf = (lv_color_t*)heap_caps_malloc(320 * 240 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (canvas_cbuf == NULL) 
    {
        ESP_LOGE("LVGL", "Failed to allocate PSRAM for canvas buffer!");
        return;
    }

    //创建画布屏幕
    ui_canvas = lv_obj_create(NULL);  
    lv_obj_clear_flag(ui_canvas, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
    LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    
    lv_obj_set_style_bg_color(ui_canvas, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_canvas, 255, LV_PART_MAIN);
    

    // 创建 LVGL 画布对象，并将其大小设置为屏幕分辨率
    drawing_canvas = lv_canvas_create(ui_canvas);
    lv_obj_set_size(drawing_canvas, 240, 320);
    
    lv_obj_add_flag(drawing_canvas, LV_OBJ_FLAG_CLICKABLE );
    // 将 PSRAM 分配的缓冲区设置给画布
    lv_canvas_set_buffer(drawing_canvas, canvas_cbuf, 240, 320, LV_IMG_CF_TRUE_COLOR);
    // 清空画布为白色，作为画板的背景
    lv_canvas_fill_bg(drawing_canvas, lv_color_hex(0x000000), LV_OPA_COVER);


    canvas_screen_return_but = lv_img_create(ui_canvas);
    lv_img_set_src(canvas_screen_return_but, &ui_img_return_png);
    lv_obj_set_width(canvas_screen_return_but, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(canvas_screen_return_but, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(canvas_screen_return_but, -75);
    lv_obj_set_y(canvas_screen_return_but, -85);
    lv_obj_add_flag(canvas_screen_return_but, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(canvas_screen_return_but, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(canvas_screen_return_but, 50);
    lv_obj_set_style_bg_color(canvas_screen_return_but, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(canvas_screen_return_but, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_set_style_img_recolor(canvas_screen_return_but, lv_color_hex(0xffffff), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(canvas_screen_return_but, 255, LV_PART_MAIN);


    lv_obj_t* canvas_screen_clean_but = lv_btn_create(ui_canvas);
    // 设置按钮的位置和大小
    lv_obj_set_size(canvas_screen_clean_but, 50, 50);
    lv_obj_align(canvas_screen_clean_but, LV_ALIGN_TOP_MID, 90, -10); // 放置在右上角

    // 创建一个标签作为按钮的文字
    lv_obj_t* canvas_screen_clean_label = lv_label_create(canvas_screen_clean_but);
    lv_label_set_text(canvas_screen_clean_label, "clean");
    lv_obj_center(canvas_screen_clean_label); // 标签居中显示在按钮上
    

    // 为按钮添加点击事件回调函数
    lv_obj_add_event_cb(canvas_screen_clean_but, clear_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(drawing_canvas, draw_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(canvas_screen_return_but, ui_canvas_return_event_cb, LV_EVENT_CLICKED, NULL);


}



static void draw_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *canvas = lv_event_get_target(e);

    // 使用静态变量来存储上一个点的坐标
    static lv_point_t prev_point;

    
    // 只在 LV_EVENT_PRESSED 和 LV_EVENT_PRESSING 事件时进行绘制
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_indev_get_act();
        if (indev == NULL) {
            return;
        }


        // 获取当前触摸点的坐标
        lv_point_t current_point;
        lv_indev_get_point(indev, &current_point);

        // 如果是首次按下，只画一个点
        if (code == LV_EVENT_PRESSED) {
            // 画一个点作为初始落笔点
            lv_canvas_set_px_color(canvas, current_point.x, current_point.y, lv_color_hex(0x13f2ef));
        } 
        // 如果是拖动，则在上一个点和当前点之间画线
        else {
            // 定义画笔样式
            static lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = lv_color_hex(0x13f2ef);
            line_dsc.width = 3;

            // 创建一个包含两个点的数组
            lv_point_t points[2];
            points[0] = prev_point;
            points[1] = current_point;
            
            // 调用正确的新版 lv_canvas_draw_line 函数
            lv_canvas_draw_line(canvas, points, 2, &line_dsc);
        }

        // 在处理完事件后，更新 prev_point，为下一次绘制做准备
        prev_point = current_point;
    }
}