#pragma once


/* I2S port and GPIOs */
#define     EXAMPLE_I2S_NUM            I2S_NUM_0
#define     EXAMPLE_I2S_MCK_IO         GPIO_NUM_38
#define     EXAMPLE_I2S_BCK_IO         GPIO_NUM_14
#define     EXAMPLE_I2S_WS_IO          GPIO_NUM_13
#define     EXAMPLE_I2S_DI_IO          GPIO_NUM_12
#define     EXAMPLE_I2S_DO_IO          GPIO_NUM_45



#define     EXAMPLE_RECV_BUF_SIZE       (2400)
#define     EXAMPLE_SAMPLE_RATE         (48000)
#define     EXAMPLE_MCLK_MULTIPLE       (256) // If not using 24-bit data width, 256 should be enough

#define     EXAMPLE_MCLK_FREQ_HZ        (EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)
#define     EXAMPLE_VOICE_VOLUME        80
#define     EXAMPLE_MODE_MUSIC          1
#define     EXAMPLE_MODE_ECHO           0



/*

   ---------------audio Input--------------------------------------------------------------------- 

*/
void es7210_app_start(void);







/*

   ---------------audio Output--------------------------------------------------------------------- 

*/
void audio_output_test(void);

