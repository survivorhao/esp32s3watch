/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include    <string.h>
#include    "sdkconfig.h"
#include    "esp_check.h"
#include    "driver/i2s_tdm.h"
#include    "driver/i2c.h"
#include    "es7210.h"
#include    "format_wav.h"
#include    "audio.h"
#include    "bsp_driver.h"



/* I2S configurations */
#define EXAMPLE_I2S_TDM_FORMAT     (ES7210_I2S_FMT_I2S)
#define EXAMPLE_I2S_CHAN_NUM       (2)
#define EXAMPLE_I2S_SAMPLE_RATE    (48000)
#define EXAMPLE_I2S_MCLK_MULTIPLE  (I2S_MCLK_MULTIPLE_256)
#define EXAMPLE_I2S_SAMPLE_BITS    (I2S_DATA_BIT_WIDTH_16BIT)
#define EXAMPLE_I2S_TDM_SLOT_MASK  (I2S_TDM_SLOT0 | I2S_TDM_SLOT1 )

/* ES7210 configurations */
#define EXAMPLE_ES7210_I2C_ADDR    (0x41)
#define EXAMPLE_ES7210_I2C_CLK     (100000)
#define EXAMPLE_ES7210_MIC_GAIN    (ES7210_MIC_GAIN_30DB)
#define EXAMPLE_ES7210_MIC_BIAS    (ES7210_MIC_BIAS_2V87)
#define EXAMPLE_ES7210_ADC_VOLUME  (32)

/* SD card & recording configurations */
#define EXAMPLE_RECORD_TIME_SEC    (10)
#define EXAMPLE_SD_MOUNT_POINT     "/sdcard"
#define EXAMPLE_RECORD_FILE_PATH   "/RECORD.WAV"

static const char *TAG = "example";

static i2s_chan_handle_t es7210_i2s_init(void)
{
    i2s_chan_handle_t i2s_rx_chan = NULL;
    ESP_LOGI(TAG, "Create I2S receive channel");

    i2s_chan_config_t i2s_rx_conf = I2S_CHANNEL_DEFAULT_CONFIG(EXAMPLE_I2S_NUM, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&i2s_rx_conf, NULL, &i2s_rx_chan));

    ESP_LOGI(TAG, "Configure I2S receive channel to TDM mode");
    
    //配置i2s mode 为Time Division Multiplexing  时分多路复用
    i2s_tdm_config_t i2s_tdm_rx_conf = {

        //声道配置,使用philips format , 16为 位宽
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(EXAMPLE_I2S_SAMPLE_BITS, I2S_SLOT_MODE_STEREO, EXAMPLE_I2S_TDM_SLOT_MASK),


        .clk_cfg  = {
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .sample_rate_hz = EXAMPLE_I2S_SAMPLE_RATE,
            .mclk_multiple = EXAMPLE_I2S_MCLK_MULTIPLE
        },

        .gpio_cfg = {
            .mclk = EXAMPLE_I2S_MCK_IO,
            .bclk = EXAMPLE_I2S_BCK_IO,
            .ws   = EXAMPLE_I2S_WS_IO,
            .dout = -1,                     // ES7210 only has ADC capability,不使用
            .din  = EXAMPLE_I2S_DI_IO
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(i2s_rx_chan, &i2s_tdm_rx_conf));

    return i2s_rx_chan;
}




static void es7210_codec_init(void)
{

    /* Create ES7210 device handle */
    es7210_dev_handle_t es7210_handle = NULL;
    es7210_i2c_config_t es7210_i2c_conf = {
        .i2c_port = EXAMPLE_I2C_NUM,
        .i2c_addr = EXAMPLE_ES7210_I2C_ADDR
    };
    ESP_ERROR_CHECK(es7210_new_codec(&es7210_i2c_conf, &es7210_handle));

    ESP_LOGI(TAG, "Configure ES7210 codec parameters");
    es7210_codec_config_t   codec_conf = {
        .i2s_format = EXAMPLE_I2S_TDM_FORMAT,
        .mclk_ratio = EXAMPLE_I2S_MCLK_MULTIPLE,
        .sample_rate_hz = EXAMPLE_I2S_SAMPLE_RATE,
        .bit_width = (es7210_i2s_bits_t)EXAMPLE_I2S_SAMPLE_BITS,
        .mic_bias = EXAMPLE_ES7210_MIC_BIAS,
        .mic_gain = EXAMPLE_ES7210_MIC_GAIN,
        .flags.tdm_enable = true
    };
    ESP_ERROR_CHECK(es7210_config_codec(es7210_handle, &codec_conf));
    ESP_ERROR_CHECK(es7210_config_volume(es7210_handle, EXAMPLE_ES7210_ADC_VOLUME));
}

static esp_err_t record_wav(i2s_chan_handle_t i2s_rx_chan)
{
    ESP_RETURN_ON_FALSE(i2s_rx_chan, ESP_FAIL, TAG, "invalid i2s channel handle pointer");
    esp_err_t ret = ESP_OK;

    uint32_t byte_rate = EXAMPLE_I2S_SAMPLE_RATE * EXAMPLE_I2S_CHAN_NUM * EXAMPLE_I2S_SAMPLE_BITS / 8;
    uint32_t wav_size = byte_rate * EXAMPLE_RECORD_TIME_SEC;

    const wav_header_t wav_header =
        WAV_HEADER_PCM_DEFAULT(wav_size, EXAMPLE_I2S_SAMPLE_BITS, EXAMPLE_I2S_SAMPLE_RATE, EXAMPLE_I2S_CHAN_NUM);

    ESP_LOGI(TAG, "Opening file %s", EXAMPLE_RECORD_FILE_PATH);
    FILE *f = fopen(EXAMPLE_SD_MOUNT_POINT EXAMPLE_RECORD_FILE_PATH, "w");
    ESP_RETURN_ON_FALSE(f, ESP_FAIL, TAG, "error while opening wav file");

    /* Write wav header */
    ESP_GOTO_ON_FALSE(fwrite(&wav_header, sizeof(wav_header_t), 1, f), ESP_FAIL, err,
                      TAG, "error while writing wav header");


    /* Start recording */
    size_t wav_written = 0;
    static int16_t i2s_readraw_buff[4096];

    //enable esp32s3 i2s rx channel to receive audio data from es7210 
    ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "error while starting i2s rx channel");
    

    while (wav_written < wav_size) {
        if (wav_written % byte_rate < sizeof(i2s_readraw_buff)) {
            ESP_LOGI(TAG, "Recording: %"PRIu32"/%ds", wav_written / byte_rate + 1, EXAMPLE_RECORD_TIME_SEC);
        }
        size_t bytes_read = 0;

        // Read RAW samples from ES7210
        ESP_GOTO_ON_ERROR(i2s_channel_read(i2s_rx_chan, i2s_readraw_buff, sizeof(i2s_readraw_buff), &bytes_read,
                                           pdMS_TO_TICKS(1000)), err, TAG, "error while reading samples from i2s");
        
                                           /* Write the samples to the WAV file */
        ESP_GOTO_ON_FALSE(fwrite(i2s_readraw_buff, bytes_read, 1, f), ESP_FAIL, err,
                          TAG, "error while writing samples to wav file");
        wav_written += bytes_read;
    }

err:
    i2s_channel_disable(i2s_rx_chan);
    ESP_LOGI(TAG, "Recording done! Flushing file buffer");
    fclose(f);

    return ret;
}

void es7210_app_start(void)
{
    /* Init I2C bus to configure ES7210 and I2S bus to receive audio data from ES7210 */
    i2s_chan_handle_t i2s_rx_chan = es7210_i2s_init();
    
    /* Create ES7210 device handle and configure codec parameters, use i2c to configure es7210 internal register */
    es7210_codec_init();
    

    /* Start to record wav audio */
    esp_err_t err = record_wav(i2s_rx_chan);
    // /* Unmount SD card */
    // esp_vfs_fat_sdcard_unmount(EXAMPLE_SD_MOUNT_POINT, sdmmc_card);
    // if (err == ESP_OK) {
    //     ESP_LOGI(TAG, "Audio was successfully recorded into "EXAMPLE_RECORD_FILE_PATH
    //              ". You can now remove the SD card safely");
    // } else 
    // {
    //     ESP_LOGE(TAG, "Record failed, "EXAMPLE_RECORD_FILE_PATH" on SD card may not be playable.");
    // }




}