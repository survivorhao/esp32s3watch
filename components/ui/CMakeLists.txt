SET(SOURCES screens/ui_lockScreen.c
    screens/ui_clock.c
    screens/ui_app1.c
    screens/ui_wifi.c
    screens/ui_weather.c
    screens/ui_app2.c
    screens/ui_camera.c
    screens/ui_canvas.c
    screens/ui_sd_browser.c
    screens/ui_calender.c
    ui_theme_manager.c
    ui_themes.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_kikyou_png.c
    images/ui_img_clock_hour_png.c
    images/ui_img_clock_min_png.c
    images/ui_img_streamlinewifi_png.c
    images/ui_img_bluetooth_png.c
    images/ui_img_music1_png.c
    images/ui_img_weather_png.c
    images/ui_img_1703540290.c
    images/ui_img_file_png.c
    images/ui_img_canvas_png.c
    images/ui_img_computer_png.c
    images/ui_img_hugeiconswifioff02_png.c
    images/ui_img_return_png.c
    images/ui_img_lightrainimage_png.c
    images/ui_img_cloudyimage2_png.c
    images/ui_img_cloudyimage1_png.c
    images/ui_img_calender_png.c
    images/ui_img_moderaterainimage_png.c
    images/ui_img_heavyrainimage_png.c
    images/ui_img_partcloudyimage_png.c
    images/ui_img_sunimage_png.c
    images/ui_img_camera_png.c
    images/ui_redtest.c
    )

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS  "."  "./screens"  "./images"
                       "./components" 
                       PRIV_REQUIRES    
                       
                       REQUIRES      lvgl   esp_lvgl_port  event
                       
                       )

