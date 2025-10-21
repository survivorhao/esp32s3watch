#include    <stdio.h>
#include    "io_extend.h"
#include    "esp_log.h"



#define     IO_EXTEND_I2C           I2C_NUM_0
#define     IO_EXTEND_ADD           0x19                  // 7-bit address


#define     LCD_CS_NUM              0             



static const char * TAG="IO_Extend";



// IO_EXTEND_IN_PORT_REG             0x00              //input port register
// IO_EXTEND_OUT_PORT_REG            0x01              //output port register
// IO_EXTEND_POLA_INVE_REG           0x02              //Polarity Inversion Register
// IO_EXTEND_CON_REG                 0x03              //configure  register 
enum    
{
    IO_EXTEND_IN_PORT_REG=0x00,
    IO_EXTEND_OUT_PORT_REG=0x01,
    IO_EXTEND_POLA_INVE_REG=0x02,
    IO_EXTEND_CON_REG=0x03,

};
esp_err_t   io_extend_write_reg(uint8_t  reg,uint8_t data);
esp_err_t   io_extend_read_reg(uint8_t  reg,uint8_t *read_buf, uint8_t read_length);
void  io_extend_set_level(uint8_t io,uint8_t level);



// // ------------------------ I2C_DRIVER_VERSION==1 Using new version of I2C driver -------------------------------------------------- //
//--------------------------------------------------------------------------------------------------------------------//
//-----------------------------------------------------------------------------------------------------------------//


/**
 * @brief Read bytes from the IO extender register over I2C.
 *
 * @param reg Register address to read from.
 * @param read_buf Buffer to receive the read data.
 * @param read_length Number of bytes to read.
 * @return ESP_OK on success or an esp_err_t on failure.
 */
esp_err_t   io_extend_read_reg(uint8_t  reg,uint8_t *read_buf, uint8_t read_length)
{
    esp_err_t ret;

    ret=i2c_master_transmit_receive( I2C_Master_io_extend_dev_handle, &reg, 1, read_buf,  read_length,  2000);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG,"---------io_extend_read_reg  fail ");

    }

    return ret;

} 

/**
 * @brief Write a single byte to an IO extender register over I2C.
 *
 * @param reg Register address to write to.
 * @param data Byte value to write.
 * @return ESP_OK on success or an esp_err_t on failure.
 */
esp_err_t   io_extend_write_reg(uint8_t  reg,uint8_t data)
{
    esp_err_t ret;

    uint8_t   write_buf[2];
    write_buf[0]=reg;
    write_buf[1]=data;

    ret=i2c_master_transmit(I2C_Master_io_extend_dev_handle,write_buf,2,2000);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG,"---------io_extend_read_reg  fail ");

    }

    return ret;

} 


//------------------------------- end --------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------------------//
//-----------------------------------------------------------------------------------------------------------------//

/**
 * @brief Initialize the PCA9557 IO extender chip and set default levels.
 *
 * Configures output port and control registers to set default IO states.
 * @return ESP_OK on success or an esp_err_t on failure.
 */
esp_err_t    io_extend_init(void)
{   
    esp_err_t ret;

    ret=io_extend_write_reg(IO_EXTEND_OUT_PORT_REG,0X05);
    
    //0xf8   1  1  1  1    1  0   0   0 
    ret=io_extend_write_reg(IO_EXTEND_CON_REG,0xf8);

    return ret;
}



/**
 * @brief Set the output level of a single IO pin on the extender.
 *
 * Reads the current output port register, updates the specified bit and
 * writes it back. Only one IO is updated per call.
 *
 * @param io IO index (0-7) to modify.
 * @param level 0 to clear, non-zero to set the bit.
 */
void  io_extend_set_level(uint8_t io,uint8_t level)
{
    uint8_t  read_buff;
    uint8_t  temp=0x01;
    io_extend_read_reg(IO_EXTEND_OUT_PORT_REG,&read_buff,1);
    if(level)
    {
        read_buff |= (temp<<io);

    }
    else
        read_buff &= ~(temp<<io);

    io_extend_write_reg(IO_EXTEND_OUT_PORT_REG,read_buff);


}


/**
 * @brief Set the LCD chip-select level via IO extender.
 *
 * Convenience wrapper that sets the configured LCD CS pin level.
 *
 * @param level 0 to drive low, non-zero to drive high.
 */
void  lcd_cs_level(uint8_t level)
{
    io_extend_set_level(LCD_CS_NUM,level);
      
}
