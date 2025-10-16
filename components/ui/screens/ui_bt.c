#include "../ui.h"
#include "ui_bt.h"


//ble state flag , 1 means ble is working
uint8_t ui_ble_working;


static char *TAG="ui_bt";

lv_obj_t *ui_bt=NULL;

lv_obj_t *ui_switch2=NULL;
lv_obj_t *ui_BLEOffImage=NULL;
lv_obj_t *ui_BLEONImage=NULL;


// 声明全局对象指针，方便后续在事件回调函数中引用
lv_obj_t *g_btn_prev;
lv_obj_t *g_btn_vol_down;
lv_obj_t *g_btn_play_pause;
lv_obj_t *g_btn_vol_up;
lv_obj_t *g_btn_next;
lv_obj_t *bt_exit_but=NULL;

#define UI_VOLUME_UP      0xe9 // Volume Increment
#define UI_VOLUME_DOWN    0xea // Volume Decrement
#define UI_PLAY_PAUSE     0xcd // Play/Pause
#define UI_CONSUMER_SCAN_NEXT_TRACK     0xb5 // scan next track 
#define UI_CONSUMER_SCAN_PREVIOUS_TRACK     0xb6 // scan previous track 


void ui_bt_control_init(void);
void ui_bt_control_delete_all(void);



void ui_event_switch2(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);

    //open ble
    if(event_code == LV_EVENT_VALUE_CHANGED &&  lv_obj_has_state(target, LV_STATE_CHECKED)) 
    {
        //if wifi is working ,ignore this request to start ble
        if(ui_wifi_working==1)
        {
            //clear switch check state
            lv_obj_clear_state(target, LV_STATE_CHECKED);

            lv_obj_add_state(target, LV_STATE_DISABLED);

            ui_ble_working=0;

            return ;

        }
        ui_ble_working=1;

        //add hiden flag
        lv_obj_add_flag(ui_BLEOffImage, LV_OBJ_FLAG_HIDDEN);      // Flags

        //remove hiden flag
        lv_obj_clear_flag(ui_BLEONImage, LV_OBJ_FLAG_HIDDEN);     /// Flags

        //post ble start event 
        ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_START, NULL, 0, portMAX_DELAY));
    }
    //close ble 
    else if(event_code == LV_EVENT_VALUE_CHANGED &&  lv_obj_has_state(target, LV_STATE_CHECKED)==false)
    {
        //remove wifi switch's disabled state, allow wifi work
        lv_obj_clear_state(ui_Switch1, LV_STATE_DISABLED);

        ui_ble_working=0;

        //add hiden flag
        lv_obj_add_flag(ui_BLEONImage, LV_OBJ_FLAG_HIDDEN);      // Flags

        //remove hiden flag
        lv_obj_clear_flag(ui_BLEOffImage, LV_OBJ_FLAG_HIDDEN);     /// Flags

        //post ble close event 
        ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_CLOSE, NULL, 0, portMAX_DELAY));

        //delete consumer control relevant widget
        ui_bt_control_delete_all();

    }
}

void ui_bt_return_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_CLICKED) 
    {

        _ui_screen_change(&ui_app1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_app1_screen_init);     
        if(ui_bt!=NULL)
        {


        }
    }
}

void ui_bt_consumer_control_event_cb(lv_event_t* e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);

    if(event_code == LV_EVENT_CLICKED) 
    {
        uint8_t control_to_send=0x00;

        if(target==g_btn_play_pause)
                control_to_send=UI_PLAY_PAUSE;
        else if(target==g_btn_vol_down)
                control_to_send=UI_VOLUME_DOWN;
        else if(target==g_btn_vol_up)
                control_to_send=UI_VOLUME_UP;
        else if(target==g_btn_prev)
                control_to_send=UI_CONSUMER_SCAN_PREVIOUS_TRACK;
        else if(target==g_btn_next)
                control_to_send=UI_CONSUMER_SCAN_NEXT_TRACK;
        else
                control_to_send=0x00;
        
        ESP_LOGI(TAG,"send control =%d", control_to_send);

        //post ble close event 
        ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle, APP_EVENT, 
                                    APP_BLE_CONSUMER_CONTROL, &control_to_send, 
                                    sizeof(control_to_send), portMAX_DELAY));
                                    
    }


}



void ui_bt_screen_init(void)
{
    //current not working
    ui_ble_working=0;


    ui_bt = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_bt, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                             LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_bt, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_bt, 255, LV_PART_MAIN);
    lv_obj_remove_style_all(ui_bt);
    
    bt_exit_but = lv_img_create(ui_bt);
    lv_img_set_src(bt_exit_but , &ui_img_return_png);
    lv_obj_set_width(bt_exit_but , LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(bt_exit_but , LV_SIZE_CONTENT);    /// 1
    lv_obj_align(bt_exit_but , LV_ALIGN_TOP_MID, -90, -80);
    lv_obj_add_flag(bt_exit_but , LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(bt_exit_but , LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                        LV_OBJ_FLAG_SNAPPABLE  | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                        LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_img_set_zoom(bt_exit_but , 50);
    lv_obj_set_style_bg_color(bt_exit_but , lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bt_exit_but , 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(bt_exit_but , lv_color_hex(0x000000), LV_PART_MAIN );
    lv_obj_set_style_img_recolor_opa(bt_exit_but , 0, LV_PART_MAIN);

    lv_obj_add_event_cb(bt_exit_but , ui_bt_return_event_cb, LV_EVENT_CLICKED, NULL);

    // --- 3. 开关 Widget (ui_switch2) ---
    ui_switch2 = lv_switch_create(ui_bt);
    lv_obj_set_width(ui_switch2, 60); // 稍微增大尺寸
    lv_obj_set_height(ui_switch2, 30);
    // 调整坐标到右上角 (例如距离右边缘 10px, 上边缘 10px)
    lv_obj_align(ui_switch2, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(ui_switch2, ui_event_switch2, LV_EVENT_ALL, NULL);

    // --- 4. 蓝牙状态标签 (Label a/b) ---
    // 根据图示，这两个标签应该创建在开关附近，并且坐标相等

    // Label A: BLE ON 状态，默认显示 LV_SYMBOL_BLUETOOTH
    ui_BLEONImage = lv_label_create(ui_bt);
    lv_label_set_text(ui_BLEONImage, "BLE ON" LV_SYMBOL_BLUETOOTH);
    // 坐标与开关 Y 居中对齐，放置在开关左侧，例如距离开关左边缘 10px
    lv_obj_align_to(ui_BLEONImage, ui_switch2, LV_ALIGN_OUT_LEFT_MID, -10, 0); 
    // 默认：假设开关默认是关的，所以 ON 标签应该隐藏
    lv_obj_add_flag(ui_BLEONImage, LV_OBJ_FLAG_HIDDEN);


    // Label B: BLE OFF 状态，我们使用一个 X 符号或 OFF 文本
    ui_BLEOffImage = lv_label_create(ui_bt);
    lv_label_set_text(ui_BLEOffImage, "BLE OFF"); // 使用文本或其他符号
    // 坐标与开关 Y 居中对齐，放置在开关左侧，与 ON 标签坐标重叠
    lv_obj_align_to(ui_BLEOffImage, ui_switch2, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    // 默认：假设开关默认是关的，所以 OFF 标签应该显示
    lv_obj_clear_flag(ui_BLEOffImage, LV_OBJ_FLAG_HIDDEN); 


}


void ui_bt_control_init(void)
{
    // 定义按钮的尺寸
    lv_coord_t btn_size = 70; // 70x70 像素，增大触摸面积
    // 定义按钮之间的间隔距离
    lv_coord_t spacing = 10; 
    
    // --- 1. 播放/暂停 (Play/Pause) 按钮 (中心) ---
    g_btn_play_pause = lv_btn_create(ui_bt);
    lv_obj_set_size(g_btn_play_pause, btn_size, btn_size); 
    lv_obj_add_flag(g_btn_play_pause, LV_OBJ_FLAG_CHECKABLE); 
    
    // 居中对齐，作为所有其他按钮的参照点
    lv_obj_align(g_btn_play_pause, LV_ALIGN_CENTER, 0 ,40);
    
    // 标签和文本：默认显示播放图标
    lv_obj_t *label_play_pause = lv_label_create(g_btn_play_pause);
    lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY); 
    lv_obj_center(label_play_pause);
    // TODO: 在这里添加事件回调

    // --- 2. 上一首 (Previous - 上方) 按钮 ---
    g_btn_prev = lv_btn_create(ui_bt);
    lv_obj_set_size(g_btn_prev, btn_size, btn_size);
    // 放置在中心按钮上方，并水平居中
    lv_obj_align_to(g_btn_prev, g_btn_play_pause, LV_ALIGN_OUT_TOP_MID, 0, -spacing); 
    lv_obj_t *label_prev = lv_label_create(g_btn_prev);
    lv_label_set_text(label_prev, LV_SYMBOL_PREV); // 使用图标代替 'pre'
    lv_obj_center(label_prev);
    
    // TODO: 在这里添加事件回调

    // --- 3. 下一首 (Next - 下方) 按钮 ---
    g_btn_next = lv_btn_create(ui_bt);
    lv_obj_set_size(g_btn_next, btn_size, btn_size);
    // 放置在中心按钮下方，并水平居中
    lv_obj_align_to(g_btn_next, g_btn_play_pause, LV_ALIGN_OUT_BOTTOM_MID, 0, spacing); 
    lv_obj_t *label_next = lv_label_create(g_btn_next);
    lv_label_set_text(label_next, LV_SYMBOL_NEXT); // 使用图标代替 'next'
    lv_obj_center(label_next);
    // TODO: 在这里添加事件回调

    // --- 4. 音量减 (Volume Down - 左方) 按钮 ---
    g_btn_vol_down = lv_btn_create(ui_bt);
    lv_obj_set_size(g_btn_vol_down, btn_size, btn_size);
    // 放置在中心按钮左侧，并垂直居中
    lv_obj_align_to(g_btn_vol_down, g_btn_play_pause, LV_ALIGN_OUT_LEFT_MID, -spacing, 0); 
    lv_obj_t *label_vol_down = lv_label_create(g_btn_vol_down);
    lv_label_set_text(label_vol_down, LV_SYMBOL_MUTE); // 使用静音图标或DOWN箭头
    lv_obj_center(label_vol_down);
    // TODO: 在这里添加事件回调
    
    // --- 5. 音量加 (Volume Up - 右方) 按钮 ---
    g_btn_vol_up = lv_btn_create(ui_bt);
    lv_obj_set_size(g_btn_vol_up, btn_size, btn_size);
    // 放置在中心按钮右侧，并垂直居中
    lv_obj_align_to(g_btn_vol_up, g_btn_play_pause, LV_ALIGN_OUT_RIGHT_MID, spacing, 0); 
    lv_obj_t *label_vol_up = lv_label_create(g_btn_vol_up);
    lv_label_set_text(label_vol_up, LV_SYMBOL_VOLUME_MAX); // 使用最大音量图标或UP箭头
    lv_obj_center(label_vol_up);
    // TODO: 在这里添加事件回调

    lv_obj_add_event_cb(g_btn_play_pause , ui_bt_consumer_control_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_btn_prev , ui_bt_consumer_control_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_btn_vol_up , ui_bt_consumer_control_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_btn_vol_down , ui_bt_consumer_control_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_btn_next , ui_bt_consumer_control_event_cb, LV_EVENT_CLICKED, NULL);
}

void ui_bt_control_delete_all(void)
{
    if(ui_bt!=NULL && g_btn_play_pause)
    {
        lv_obj_del_delayed(g_btn_play_pause,100);      // Flags
        g_btn_play_pause=NULL;

        lv_obj_del_delayed(g_btn_prev, 100);      // Flags
        lv_obj_del_delayed(g_btn_next, 100);      // Flags
        lv_obj_del_delayed(g_btn_vol_down, 100);      // Flags
        lv_obj_del_delayed(g_btn_vol_up, 100);      // Flags


    }

}



void ui_bt_control_hide_all(void)
{
    if(ui_bt!=NULL && g_btn_play_pause)
    {
        lv_obj_add_flag(g_btn_play_pause, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_add_flag(g_btn_prev, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_add_flag(g_btn_next, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_add_flag(g_btn_vol_down, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_add_flag(g_btn_vol_up, LV_OBJ_FLAG_HIDDEN);      // Flags
    }

}


void ui_bt_control_display_all(void)
{
    if(ui_bt!=NULL && g_btn_play_pause)
    {
        lv_obj_clear_flag(g_btn_play_pause, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_clear_flag(g_btn_prev, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_clear_flag(g_btn_next, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_clear_flag(g_btn_vol_down, LV_OBJ_FLAG_HIDDEN);      // Flags
        lv_obj_clear_flag(g_btn_vol_up, LV_OBJ_FLAG_HIDDEN);      // Flags
    }

}