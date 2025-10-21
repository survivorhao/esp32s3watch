#pragma once
#include    "esp_err.h"
#include    "bsp_driver.h"




/**
 * @brief Initialize the PCA9557 IO extender chip and set default levels.
 *
 * Configures output port and control registers to set default IO states.
 * @return ESP_OK on success or an esp_err_t on failure.
 */
esp_err_t    io_extend_init(void);





/**
 * @brief Set the output level of a single IO pin on the extender.
 *
 * Reads the current output port register, updates the specified bit and
 * writes it back. Only one IO is updated per call.
 *
 * @param io IO index (0-7) to modify.
 * @param level 0 to clear, non-zero to set the bit.
 */
void  io_extend_set_level(uint8_t io,uint8_t level);



/**
 * @brief Set the LCD chip-select level via IO extender.
 *
 * Convenience wrapper that sets the configured LCD CS pin level.
 *
 * @param level 0 to drive low, non-zero to drive high.
 */
void  lcd_cs_level(uint8_t level);
