#include    <stdio.h>
#include    <stdint.h>
#include    <sys/unistd.h>
#include    <sys/stat.h>
#include    "esp_vfs_fat.h"
#include    "sdmmc_cmd.h"
#include    "driver/sdmmc_host.h"
#include    "esp_log.h"
#include    <string.h>
#include    <errno.h>
#include    <string.h>
#include    <sys/types.h>
#include    <dirent.h>
#include    "esp_heap_caps.h"
#include    "lvgl.h"
#include    "esp_lvgl_port.h"
#include    <inttypes.h>

#include    "sdcard.h"
#include    "bsp_driver.h"
#include    "event.h"
#include    "ui.h"

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

const char *TAG="my_sdcard";

static bool sd_mount_success=0;


// 假设 LCD 分辨率
#define LCD_H_RES 240
#define LCD_V_RES 320

#define     EXAMPLE_MAX_CHAR_SIZE    64

#define     SD_CMD_PIN                 GPIO_NUM_48
#define     SD_CLK_PIN                 GPIO_NUM_47
#define     SD_DATA0_PIN               GPIO_NUM_21

#define     MOUNT_POINT                "/sdcard"

#define     SD_PIC_DIR                 "/sdcard/pic/"

//指向存放img data数据的Buffer,分配在heap上
uint16_t *img_data=NULL;


// BMP Header (14 bytes)
typedef struct {
    uint16_t bfType;      // 0x4D42 ('BM')
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;   // 像素数据偏移
} __attribute__((packed)) bmp_file_header_t;    //强制不内存对齐


// BITMAPV5HEADER (124 bytes)
typedef struct {
    uint32_t biSize;          // DIB头部字段的size 应该为124 Byte =0x7c
    int32_t  biWidth;
    int32_t  biHeight;        // biHeight >0 从下到上， biHeight<0 从上到下
    uint16_t biPlanes;        // 1
    uint16_t biBitCount;      // 16, 24, 32
    uint32_t biCompression;   // 0 = BI_RGB (无压缩)
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
    // V5 扩展字段 (40 bytes more to make 124)
    uint32_t biRedMask;
    uint32_t biGreenMask;
    uint32_t biBlueMask;
    uint32_t biAlphaMask;
    uint32_t biCSType;
    int32_t  biIntent;
    uint32_t biCSTransIn;
    uint32_t biCSTransOut;
    uint32_t biCSTransSrc;
    uint32_t biCSTransDesc;
    uint32_t biCSTransAlpha;
    uint32_t biCSTransProfile;
    uint32_t biReserved;
} __attribute__((packed)) bmp_dib_header_t;         //强制不内存对齐


   // Add this new structure for 40-byte DIB header
typedef struct {
    uint32_t biSize;          // 40
    int32_t  biWidth;
    int32_t  biHeight;        // biHeight >0 从下到上， biHeight<0 从上到下
    uint16_t biPlanes;        // 1
    uint16_t biBitCount;      // 16, 24, 32
    uint32_t biCompression;   // 0 = BI_RGB (无压缩) or 3 = BI_BITFIELDS
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} __attribute__((packed)) bmp_info_header_t;  // 强制不内存对齐


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


void list_dir(const char *path);

static esp_err_t read_bmp_and_display(const char *file_path) ;
static uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b);

static void user_sd_refresh_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}


/*
    读出指定path路径下的内容

*/
static esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) 
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}


/*
    初始化sd卡底层驱动函数并且挂载 fat 文件系统


*/
void sdcard_init_mount_fs(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {

        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    

    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    ESP_LOGI(TAG, "Using SDMMC peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Set bus width to use,使用一个data 线
    slot_config.width = 1;

    // On chips where the GPIOs used for SD card can be configured, set them in
    // the slot_config structure:
    slot_config.clk = SD_CLK_PIN;               //clock pin 
    slot_config.cmd = SD_CMD_PIN;               //command pin
    slot_config.d0 = SD_DATA0_PIN;              //data  0  



    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    //存放sd卡信息
    sdmmc_card_t *card;
    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) 
    {
        if (ret == ESP_FAIL) 
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else 
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    
    //change flag 
    sd_mount_success=1;

    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_FILE_REFRESH_REQUEST, user_sd_refresh_handler, NULL));

}


void list_dir(const char *path)
{
    ESP_LOGI(TAG, "Listing files in %s:", path);

    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
        return;
    }

    printf("%s:\n", path);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf(
            "    %s: %s\n",
            (entry->d_type == DT_DIR)
                ? "directory"
                : "file     ",
            entry->d_name
        );
    }

    closedir(dir);
}




// RGB888 到 RGB565 转换 (忽略 Alpha for 32-bit)
static uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b) 
{   
    volatile uint16_t data=((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);   
    //交换高低字节
    return  ((((uint8_t)data)<<8)|((uint8_t)(data>>8)));

}



static esp_err_t read_bmp_and_display(const char *file_path) 
{
    FILE *f = fopen(file_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open BMP file: %s", file_path);
        return ESP_FAIL;
    }

    bmp_file_header_t file_hdr;
    
    // 头部字段不对，直接返回
    if (fread(&file_hdr, sizeof(bmp_file_header_t), 1, f) != 1 || file_hdr.bfType != 0x4D42) 
    {
        ESP_LOGE(TAG, "Invalid BMP file header");
        fclose(f);
        return ESP_FAIL;
    }

    // 先读取 DIB Header 的 biSize (4 bytes) 来判断头部大小
    uint32_t biSize;
    if (fread(&biSize, sizeof(uint32_t), 1, f) != 1) {
        ESP_LOGE(TAG, "Failed to read DIB header size");
        fclose(f);
        return ESP_FAIL;
    }
    fseek(f, -sizeof(uint32_t), SEEK_CUR);  // 回退到 DIB Header 开始位置

    // 公共字段
    int width, height;
    uint16_t bit_count;
    uint32_t compression;
    bool is_bottom_up;  // 注意：这里命名保持原样，但实际表示是否 top-down (biHeight < 0)

    //124byte DIB Header
    if (biSize == 124) 
    {
        // 读取 V5 Header (124 bytes)
        bmp_dib_header_t dib_hdr;
        if (fread(&dib_hdr, sizeof(bmp_dib_header_t), 1, f) != 1 || dib_hdr.biPlanes != 1) {
            ESP_LOGE(TAG, "Unsupported DIB header (must be V5)");
            fclose(f);
            return ESP_FAIL;
        }
        compression = dib_hdr.biCompression;
        if (compression != 0 ) 
        {
            ESP_LOGE(TAG, "Unsupported compression: %"PRIu32" (must be 0 )", compression);
            fclose(f);
            return ESP_FAIL;
        }
        width = abs(dib_hdr.biWidth);
        height = abs(dib_hdr.biHeight);
        bit_count = dib_hdr.biBitCount;
        is_bottom_up = (dib_hdr.biHeight < 0);

        ESP_LOGW(TAG,"read bmp width is %d,height is %d", width, height);
    }

    //40byte DIB Header
    else if (biSize == 40) 
    {
        // 读取 INFO Header (40 bytes)
        bmp_info_header_t dib_hdr;
        if (fread(&dib_hdr, sizeof(bmp_info_header_t), 1, f) != 1 || dib_hdr.biPlanes != 1) 
        {
            ESP_LOGE(TAG, "Unsupported DIB header (must be INFO)");
            fclose(f);
            return ESP_FAIL;
        }
        compression = dib_hdr.biCompression;
        if (compression != 0 ) 
        {
            ESP_LOGE(TAG, "Unsupported compression: %"PRIu32" (must be 0 )", compression);
            fclose(f);
            return ESP_FAIL;
        }
        width = abs(dib_hdr.biWidth);
        height = abs(dib_hdr.biHeight);
        bit_count = dib_hdr.biBitCount;
        is_bottom_up = (dib_hdr.biHeight < 0);

        ESP_LOGW(TAG,"read bmp width is %d,height is %d", width, height);
    }
    else 
    {
        ESP_LOGE(TAG, "Unsupported DIB header size: %"PRIu32" (must be 40 or 124)", biSize);
        fclose(f);
        return ESP_FAIL;
    }

    if (width > LCD_H_RES || height > LCD_V_RES)
    {
        ESP_LOGE(TAG, "BMP size too large, mismatch: %dx%d (expected %dx%d)", width, height, LCD_H_RES, LCD_V_RES);
        fclose(f);
        return ESP_FAIL;
    }

    // 颜色深度
    if (bit_count < 16) 
    {
        ESP_LOGE(TAG, "Unsupported bit depth: %"PRIu16"(<16)", bit_count);
        fclose(f);
        return ESP_FAIL;
    }

    // 跳到像素数据
    fseek(f, file_hdr.bfOffBits, SEEK_SET);

    // 分配 RGB565 缓冲 (使用 PSRAM)
    if(img_data !=NULL)
    {
        //释放上次在heap上分配的memory
        free(img_data);
        img_data=NULL;

    }
    img_data= (uint16_t *)heap_caps_malloc(width * height * sizeof(uint16_t),
                                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!img_data) 
    {
        ESP_LOGE(TAG, "Failed to allocate image buffer");
        fclose(f);
        return ESP_FAIL;
    }

    // 计算每行 padding (BMP 行以 4 字节对齐)
    int bytes_per_pixel = bit_count / 8;
    int row_bytes = width * bytes_per_pixel;
    int padding = (4 - (row_bytes % 4)) % 4;
    int row_size_with_padding = row_bytes + padding;  // 整行大小，包括填充

    // 分配临时行缓冲区（使用 PSRAM 以节省内部 RAM）
    uint8_t *row_buffer = (uint8_t *)heap_caps_malloc(row_size_with_padding, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!row_buffer) {
        ESP_LOGE(TAG, "Failed to allocate row buffer");
        heap_caps_free(img_data);
        img_data = NULL;
        fclose(f);
        return ESP_FAIL;
    }

    // 为真表示从上到下存储,从bmp文件中先读出来的数据 是实际图像的上方数据,应该放在Buff缓冲的首部
    // 为假表示从下到上存储,从bmp文件中先读出来的数据 是实际图像的下方数据,应该放在Buff缓冲的尾部
    int y_start = is_bottom_up ? 0 : height - 1;
    int y_step = is_bottom_up ? 1 : -1;

    uint16_t test_point=0;
    for (int y = 0; y < height; y++)
    {
        // 一次读取整行数据，包括填充,单次读取太少反复io浪费资源
        if (fread(row_buffer, row_size_with_padding, 1, f) != 1) {
            ESP_LOGE(TAG, "Failed to read row data");
            heap_caps_free(row_buffer);
            heap_caps_free(img_data);
            img_data = NULL;
            fclose(f);
            return ESP_FAIL;
        }

        int lv_y = (is_bottom_up ? y : height - 1 - y);  
        uint16_t *dest_ptr = &img_data[lv_y * width];  // 目标行指针

        if (bit_count == 16) 
        {
            memcpy(dest_ptr, row_buffer, row_bytes);
        }
        else if (bit_count == 24) 
        {
            // 24-bit: BGR888 转换到 RGB565
            uint8_t *src_ptr = row_buffer;
            for (int x = 0; x < width; x++) 
            {
                uint8_t b = src_ptr[x * 3 + 0];
                uint8_t g = src_ptr[x * 3 + 1];
                uint8_t r = src_ptr[x * 3 + 2];
                dest_ptr[x] = rgb888_to_565(r, g, b);
            }
        }
        else if (bit_count == 32) 
        {
            // 32-bit: BGRA8888 转换到 RGB565 (忽略 A)
            uint8_t *src_ptr = row_buffer;
            for (int x = 0; x < width; x++) 
            {
                uint8_t b = src_ptr[x * 4 + 0];
                uint8_t g = src_ptr[x * 4 + 1];
                uint8_t r = src_ptr[x * 4 + 2];
                // uint8_t a = src_ptr[x * 4 + 3];  // 忽略
                dest_ptr[x] = rgb888_to_565(r, g, b);
            }
        }
    }

    // 释放行缓冲区
    heap_caps_free(row_buffer);
    fclose(f);

    ESP_LOGI(TAG,"bmp data load finished \n");

    //make sure lvgl api thread safe 
    if(lvgl_port_lock(500)==true)
    {
        // 创建 LVGL 图像 widget (RGB565)
        static  lv_img_dsc_t img_dsc;
        img_dsc.header.always_zero = 0,
        img_dsc.header.w = width,
        img_dsc.header.h = height,
        img_dsc.data_size = width * height * 2,  // 16-bit
        img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR,  // RGB565
        img_dsc.data = (const uint8_t *)img_data,
        lv_img_set_src(image_display, &img_dsc);

        //切换屏幕
        lvgl_port_unlock();
        ESP_LOGI(TAG, "BMP loaded and displayed (bit depth: %"PRIu16") \n", bit_count);
    }
    else 
    {
        ESP_LOGE(TAG,"take lvgl mutex fail (file %s,func %s,line %d)\n",__FILE__, __func__, __LINE__);

    }
    // 注意: img_data 需在 widget 销毁时释放 (可选添加清理任务)
    return ESP_OK;
}




static void user_sd_refresh_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    if(!sd_mount_success)
    {
        //未挂载sd卡，直接返回error
        file_refresh_res_data_t data={
            .current_path="please mount sd first",

            .item_count=0,
            .items=NULL,
            .err=ESP_OK,

        };

        //发送file list refresh 完成数据
        esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_FILE_REFRESH_RESPONSE, &data, sizeof(data), portMAX_DELAY);
        return ;
    }

    file_refresh_req_data_t *data=(file_refresh_req_data_t* )event_data;

    //存放file list refresh 响应数据
    file_refresh_res_data_t result; 
    
    //点击的是目录，列出指定目录下的内容
    if(data->is_directory)
    {
        //refresh data->name !=0 表明这是点击子目录发送的刷新请求
        if(strlen(data->name)!=0)
        {
            strcat(data->current_path, data->name);
            strcat(data->current_path, "/");            
        }
        ESP_LOGI(TAG,"refresh target full path is %s ",data->current_path);

        //更改current path
        strcpy(result.current_path, data->current_path);

        // 读取目录
        DIR *dir = opendir(data->current_path);
        if (dir) 
        {
            struct dirent *entry;
            uint8_t capacity = 10;  // 初始容量

            result.items = heap_caps_malloc(capacity * sizeof(file_item_t), MALLOC_CAP_SPIRAM |MALLOC_CAP_8BIT);
            result.item_count = 0;

            

            while ((entry = readdir(dir)) != NULL) 
            {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

                if (result.item_count >= capacity) 
                {
                    capacity *= 2;
                    result.items = heap_caps_realloc(result.items, capacity * sizeof(file_item_t), MALLOC_CAP_SPIRAM |MALLOC_CAP_8BIT);
                }

                strcpy(result.items[result.item_count].name, entry->d_name);
                char *full_path=heap_caps_malloc(sizeof(uint8_t )* 512, MALLOC_CAP_SPIRAM |MALLOC_CAP_8BIT);


                uint8_t  actual_num=snprintf(full_path, 512, "%s%s", data->current_path, entry->d_name);
                full_path[actual_num]='\0';
                
                struct stat st;

                //判断是否是目录
                result.items[result.item_count].is_dir = (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode));
                result.item_count++;

                //释放
                free(full_path);
            }
            closedir(dir);
            result.err = ESP_OK;
        
            // ESP_LOGI(TAG,"browser finish,now full path is %s: ",result.current_path);

            esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_FILE_REFRESH_RESPONSE, &result, sizeof(result), portMAX_DELAY);

        }
        else 
        {
            result.err = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to open dir: %s,please check whether actually have \n", result.current_path);
        }



    }
    //点击的是文件，判断文件类型
    else
    {

        //bmp 图片，不支持解析png图片！！！
        if (strcasecmp(data->ext_name, "bmp") == 0)
        {

            char *file_path=strcat(data->current_path, data->name);
            // ESP_LOGI(TAG,"display picture full path is %s \n", data->current_path);
            if(lvgl_port_lock(500)==true)
            {
                _ui_screen_change(&ui_image_display, LV_SCR_LOAD_ANIM_MOVE_LEFT, 0, 0, ui_image_display_screen_init);
                lvgl_port_unlock();
            }
            else
            {
                ESP_LOGE(TAG,"take lvgl mutex fail (file %s,func %s,line %d)\n",__FILE__, __func__, __LINE__);

            }

            esp_err_t ret=read_bmp_and_display(file_path);
            if(ret!= ESP_OK)
            {
                ESP_LOGE(TAG,"display picture fail ");

            }
        }
        

    }

}