#ifndef UI_SD_BROWSER_H
#define UI_SD_BROWSER_H

#ifdef __cplusplus
extern "C" {
#endif


void ui_sd_browser_screen_init(void);

void file_list_click_cb(lv_event_t *e) ;

void ui_image_display_screen_init(void);
//图片显示屏幕
extern lv_obj_t* ui_image_display;
extern lv_obj_t* image_display_ret_but;
extern lv_obj_t* image_display;


//sd_browser screen屏幕
extern lv_obj_t* ui_sd_browser;
extern lv_obj_t* sd_browser_ret_but;

//当前路径
extern lv_obj_t* main_path_label;

//当前路径
extern lv_obj_t* branch_path_label;
 
//当前路径下的list
extern lv_obj_t* file_list;


extern char sd_current_dir[SD_PATH_MAX];
extern char branch_sd_current_dir[SD_PATH_MAX];








#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
