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


esp_err_t   io_extend_read_reg(uint8_t  reg,uint8_t *read_buf, uint8_t read_length)
{
    esp_err_t ret;

    uint8_t   write_buf;
    write_buf=reg;

    ret=i2c_master_transmit_receive( I2C_Master_io_extend_dev_handle, &reg, 1, read_buf,  read_length,  2000);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG,"---------io_extend_read_reg  fail ");

    }

    return ret;

} 

/*
    @reg:   io_extend chip  specific  register

    @data:  data to write 


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
/*
    初始化PCA9557 IO扩展芯片，以及配置使用的几个拓展io的默认电平


*/
esp_err_t    io_extend_init(void)
{   
    esp_err_t ret;
    uint8_t   config_reg;


    ret=io_extend_write_reg(IO_EXTEND_OUT_PORT_REG,0X05);
    
    //0xf8   1  1  1  1    1  0   0   0 
    ret=io_extend_write_reg(IO_EXTEND_CON_REG,0xf8);

    return ret;
}



/*
    @desc: 设置io拓展芯片的io的输出电平，注意此函数一次调用仅设置一个io

    @io：   指定要设置的io序号，0-7

    @level: 要输出的电平，  0或者1


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


void  lcd_cs_level(uint8_t level)
{
      io_extend_set_level(LCD_CS_NUM,level);
      

}
