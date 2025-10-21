#pragma once

#include    "esp_vfs_fat.h"
#include    "sdmmc_cmd.h"
#include    "driver/sdmmc_host.h"


#define     SD_CMD_PIN                 GPIO_NUM_48
#define     SD_CLK_PIN                 GPIO_NUM_47
#define     SD_DATA0_PIN               GPIO_NUM_21




/**
 * @brief Initialize SDMMC host, mount FAT filesystem 
 *          and register handlers.
 *
 * Initializes the SDMMC host and slot configuration for a 1-line SD
 * interface, mounts the FAT filesystem at MOUNT_POINT (formatting it if
 * necessary), prints card info and registers event handlers for file
 * refresh and camera picture deletion.
 *
 * This function sets the global @c sd_mount_success flag when mounting
 * succeeds and populates the global @c card pointer with the detected
 * card information.
 */
void sdcard_init_mount_fs(void);

//indicate sd mount status, 1 means mount success
extern bool sd_mount_success;


extern sdmmc_card_t *card;
















