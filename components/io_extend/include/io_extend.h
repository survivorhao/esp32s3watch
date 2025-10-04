#pragma once
#include    "esp_err.h"
#include    "bsp_driver.h"




/*
    初始化PCA9557 IO扩展芯片，以及配置使用的几个拓展io的默认电平


*/
esp_err_t    io_extend_init(void);

void io_extend_my_test(void);



/*
    @desc: 设置io拓展芯片的io的输出电平

    @io：   指定要设置的io序号，0-7

    @level: 要输出的电平，  0或者1


*/
void  io_extend_set_level(uint8_t io,uint8_t level);


void  lcd_cs_level(uint8_t level);
