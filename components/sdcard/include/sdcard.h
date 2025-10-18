#pragma once

#include    "esp_vfs_fat.h"
#include    "sdmmc_cmd.h"
#include    "driver/sdmmc_host.h"

/*
    初始化sd卡底层驱动函数并且挂载 fat 文件系统


*/
void sdcard_init_mount_fs(void);

extern bool sd_mount_success;

extern sdmmc_card_t *card;
















