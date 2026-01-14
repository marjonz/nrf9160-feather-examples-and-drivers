/*****************************************************************************
* | File      	:  	EPD_4in26.c
* | Author      :   Waveshare team
* | Function    :   4.26inch e-paper test demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2023-12-19
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************
from : https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_4in26.c
******************************************************************************/
#include "epaper_driver.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(epaper_driver, LOG_LEVEL_DBG);

//#define DISABLE_BUSY_CHECK_FOR_DEBUGGING    true

const unsigned char LUT_DATA_4Gray[112] =    //112bytes
{											
    0x80u,	0x48u,	0x4Au,	0x22u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x0Au,	0x48u,	0x68u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x88u,	0x48u,	0x60u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0xA8u,	0x48u,	0x45u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x07u,	0x1Eu,	0x1Cu,	0x02u,	0x00u,						
    0x05u,	0x01u,	0x05u,	0x01u,	0x02u,						
    0x08u,	0x01u,	0x01u,	0x04u,	0x04u,						
    0x00u,	0x02u,	0x00u,	0x02u,	0x01u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x01u,						
    0x22u,	0x22u,	0x22u,	0x22u,	0x22u,						
    0x17u,	0x41u,	0xA8u,	0x32u,	0x30u,						
    0x00u,	0x00u,	
};	

#define EPAPER_DEVICE_NODE_ID DT_NODELABEL(epaper_device)
static const struct device *spi_dev = DEVICE_DT_GET(DT_BUS(EPAPER_DEVICE_NODE_ID));
static const struct spi_dt_spec spi = 
    SPI_DT_SPEC_GET(
        EPAPER_DEVICE_NODE_ID, 
        (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB), 
        0
    );

#define INACTIVE_LOGIC          0
#define ACTIVE_LOGIC            1   
#define DATA_CMD_IS_COMMAND     0
#define DATA_CMD_IS_DATA        1

#define SPI_EPAPER_NODE_ID DT_NODELABEL(epaper_device)
// Reset pin is active low, so idle high.
static const struct gpio_dt_spec reset_gpio = GPIO_DT_SPEC_GET(EPAPER_DEVICE_NODE_ID, rst_gpios);
static const struct gpio_dt_spec data_cmd_gpio = GPIO_DT_SPEC_GET(EPAPER_DEVICE_NODE_ID, data_command_gpios);
static const struct gpio_dt_spec busy_gpio = GPIO_DT_SPEC_GET(EPAPER_DEVICE_NODE_ID, busy_gpios);
static const struct gpio_dt_spec power_gpio = GPIO_DT_SPEC_GET(EPAPER_DEVICE_NODE_ID, pwr_gpios);

// Turn on ePaper power supply.
#define POWER_ON()  do { \
                        gpio_pin_set_dt(&power_gpio, ACTIVE_LOGIC); \
                        k_msleep(10);                               \
                    } while (0)

// Turn off ePaper power supply.
#define POWER_OFF() gpio_pin_set_dt(&power_gpio, INACTIVE_LOGIC);

// NOTE: For some reason, the RESET output pin's logic is inverted in hardware.
// When writing 1 to the pin, the output is low, and vice versa.
#define RESET_ACTIVE()   do { \
                                gpio_pin_set_dt(&reset_gpio, ACTIVE_LOGIC); \
                         } while (0)

#define RESET_INACTIVE()   do { \
                                gpio_pin_set_dt(&reset_gpio, INACTIVE_LOGIC); \
                         } while (0)

#define SEND_COMMAND()   do { \
                                gpio_pin_set_dt(&data_cmd_gpio, DATA_CMD_IS_COMMAND); \
                         } while (0)

#define SEND_DATA()   do { \
                                gpio_pin_set_dt(&data_cmd_gpio, DATA_CMD_IS_DATA); \
                      } while (0)
                            

#define TX_BUFFER_SIZE 120 
#define RX_BUFFER_SIZE 20 
// NOTE: The ePaper does not have a MOSI line, to it can't transmit data out.
static uint8_t tx_buf_data[TX_BUFFER_SIZE] = { 0 };
static struct spi_buf tx_buf = 
{
    .buf = tx_buf_data,
    .len = sizeof(tx_buf_data),
};
static struct spi_buf_set tx = 
{
    .buffers = &tx_buf,
    .count = 1,
};

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_4in26_Reset(void)
{
    RESET_INACTIVE();
    k_msleep(100);
    RESET_ACTIVE();
    k_msleep(2);
    RESET_INACTIVE();
    k_msleep(100);
}

/******************************************************************************
 * Helper function to send n bytes over SPI
 *****************************************************************************/
static void send_n_bytes(uint8_t *data, size_t len)
{
    if (spi_dev != NULL) 
    {
        memset(tx_buf_data, 0, sizeof(tx_buf_data));
        size_t data_len = len < TX_BUFFER_SIZE ? len : sizeof(tx_buf_data);
        memcpy(tx_buf_data, data, data_len);
        tx_buf.len = data_len;
        int ret = spi_write_dt(&spi, &tx);
        if (ret != 0) 
        {
            LOG_DBG("SPI command transfer failed: %d", ret);
        }
    }
    else 
    {
        LOG_DBG("SPI device not initialized");
    }
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
static void EPD_4in26_SendCommand(uint8_t Reg)
{
    LOG_DBG("EPD_4in26_SendCommand: 0x%02X", Reg);
    SEND_COMMAND();
    send_n_bytes(&Reg, sizeof(Reg));
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
static void EPD_4in26_SendData(uint8_t Data)
{
    LOG_DBG("EPD_4in26_SendData: 0x%02X", Data);
    SEND_DATA();
    send_n_bytes(&Data, 1);
}

static void EPD_4in26_SendData2(uint8_t *pData, size_t len)
{
    LOG_DBG("EPD_4in26_SendData2: 0x%02X of len: %d", pData[0], len);
    SEND_DATA();
    send_n_bytes(pData, len);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
#ifndef DISABLE_BUSY_CHECK_FOR_DEBUGGING
static inline bool is_busy(void)
{
    return (gpio_pin_get_dt(&busy_gpio) > INACTIVE_LOGIC);
}

void EPD_4in26_ReadBusy(void)
{
    bool busy_status = is_busy();
    while(busy_status != 0)
	{	 //=1 BUSY (ACTIVE HIGH)
        LOG_DBG("e-Paper busy: %d", busy_status);
		k_msleep(20);
        busy_status = is_busy();
	}
	k_msleep(20);
    LOG_DBG("e-Paper busy release: %d", busy_status);
}

void EPD_4in26_ReadBusy_Debug(void)
{
    bool busy_status = is_busy();
    while(busy_status)
	{	 //=1 BUSY (ACTIVE HIGH)
        LOG_DBG("DBG busy: %d", busy_status);
		k_msleep(1000);
        busy_status = is_busy();
	}
	k_msleep(1000);
    LOG_DBG("DBG release: %d", busy_status);
}

#else
void EPD_4in26_ReadBusy(void)
{
    LOG_DBG("e-Paper busy release");
}
#endif

/******************************************************************************
function :	Helper function to send display update data
parameter:
******************************************************************************/
static inline void send_display_update_data(uint8_t data)
{
	EPD_4in26_SendCommand(0x22u); //Display Update Control
	EPD_4in26_SendData(data);
	EPD_4in26_SendCommand(0x20u); //Activate Display Update Sequence
	EPD_4in26_ReadBusy();
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
static void EPD_4in26_TurnOnDisplay(void)
{
    send_display_update_data(0xF7u);
}

static void EPD_4in26_TurnOnDisplay_Fast(void)
{
    send_display_update_data(0xC7u);
}

static void EPD_4in26_TurnOnDisplay_Part(void)
{
    send_display_update_data(0xFFu);
}

static void EPD_4in26_TurnOnDisplay_4GRAY(void)
{
    send_display_update_data(0xC7u);
}

/******************************************************************************
function :	set the look-up tables
parameter:
******************************************************************************/
static void EPD_4in26_Lut(void)
{
    uint16_t count;
    EPD_4in26_SendCommand(0x32u); //vcom
    for(count = 0u; count < 105u ; count++) 
    {
        EPD_4in26_SendData(LUT_DATA_4Gray[count]);
    }

    EPD_4in26_SendCommand(0x03u); //VGH      
	EPD_4in26_SendData(LUT_DATA_4Gray[105u]);

	EPD_4in26_SendCommand(0x04); //      
	EPD_4in26_SendData(LUT_DATA_4Gray[106u]); //VSH1   
	EPD_4in26_SendData(LUT_DATA_4Gray[107u]); //VSH2   
	EPD_4in26_SendData(LUT_DATA_4Gray[108u]); //VSL   

	EPD_4in26_SendCommand(0x2Cu);     //VCOM Voltage
	EPD_4in26_SendData(LUT_DATA_4Gray[109u]);    //0x1C
}

static inline void send_position_data(uint16_t pos)
{
    EPD_4in26_SendData(pos & 0xFFu);
    EPD_4in26_SendData((pos >> 8u) & 0x03u);
}

/******************************************************************************
function :	Setting the display window
parameter:
******************************************************************************/
static void EPD_4in26_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend)
{
    EPD_4in26_SendCommand(0x44u); // SET_RAM_X_ADDRESS_START_END_POSITION
    send_position_data(Xstart);
    send_position_data(Xend);
	
    EPD_4in26_SendCommand(0x45u); // SET_RAM_Y_ADDRESS_START_END_POSITION
    send_position_data(Ystart);
    send_position_data(Yend);
}

/******************************************************************************
function :	Set Cursor
parameter:
******************************************************************************/
static void EPD_4in26_SetCursor(uint16_t Xstart, uint16_t Ystart)
{
    EPD_4in26_SendCommand(0x4Eu); // SET_RAM_X_ADDRESS_COUNTER
    send_position_data(Xstart);

    EPD_4in26_SendCommand(0x4Fu); // SET_RAM_Y_ADDRESS_COUNTER
    send_position_data(Ystart);
}

static void configure_pins_and_power_on(void)
{
    if (!spi_is_ready_dt(&spi)) 
    {
        LOG_ERR("EPAPER SPI not ready");
        return;
    }

    
    gpio_pin_configure_dt(&power_gpio, GPIO_OUTPUT_INACTIVE);
    if (!device_is_ready(power_gpio.port)) 
    {
        LOG_ERR("EPAPER POWER GPIO not ready");
        return;
    }
    POWER_ON();

    gpio_pin_configure_dt(&reset_gpio, GPIO_OUTPUT_HIGH);
    if (!device_is_ready(reset_gpio.port)) 
    {
        LOG_ERR("EPAPER RESET GPIO not ready");
        return;
    }
    RESET_INACTIVE();

    gpio_pin_configure_dt(&data_cmd_gpio, GPIO_OUTPUT_LOW);
    if (!device_is_ready(data_cmd_gpio.port)) 
    {
        LOG_ERR("EPAPER DATA/CMD GPIO not ready");
        return;
    }
    
    if (!gpio_is_ready_dt(&busy_gpio)) 
    {
        LOG_ERR("EPAPER BUSY GPIO not ready");
        return;
    }
    gpio_pin_configure_dt(&busy_gpio, GPIO_INPUT | GPIO_PULL_DOWN);

    LOG_DBG("EPAPER GPIO ready and device is powered on.");
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_4in26_Init(void)
{
    configure_pins_and_power_on();

	EPD_4in26_Reset();
    LOG_DBG("EPAPER device reset. Init");
	k_msleep(100);

	EPD_4in26_ReadBusy();   
	EPD_4in26_SendCommand(0x12u);  //SWRESET
	EPD_4in26_ReadBusy();   
	
	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x0Cu); //set soft start     
	EPD_4in26_SendData(0xAEu);
	EPD_4in26_SendData(0xC7u);
	EPD_4in26_SendData(0xC3u);
	EPD_4in26_SendData(0xC0u);
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x01u);   //      drive output control    
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1)%256); //  Y  
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1)/256); //  Y 
	EPD_4in26_SendData(0x02);

	EPD_4in26_SendCommand(0x3C);        // Border       Border setting 
	EPD_4in26_SendData(0x01);

	EPD_4in26_SendCommand(0x11);        //    data  entry  mode
	EPD_4in26_SendData(0x01);           //       X-mode  x+ y-    

	EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT-1, EPD_4in26_WIDTH-1, 0);

	EPD_4in26_SetCursor(0, 0);

	EPD_4in26_ReadBusy();
}

void EPD_4in26_Init_Fast(void)
{
    configure_pins_and_power_on();

	EPD_4in26_Reset();
    LOG_DBG("EPAPER device reset. Init Fast");
	k_msleep(100);

	EPD_4in26_ReadBusy();   
	EPD_4in26_SendCommand(0x12u);  //SWRESET
	EPD_4in26_ReadBusy();   
	
	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x0Cu); //set soft start     
	EPD_4in26_SendData(0xAEu);
	EPD_4in26_SendData(0xC7u);
	EPD_4in26_SendData(0xC3u);
	EPD_4in26_SendData(0xC0u);
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x01u);   //      drive output control    
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1) % 256); //  Y  
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1) / 256); //  Y 
	EPD_4in26_SendData(0x02u);

	EPD_4in26_SendCommand(0x3Cu);        // Border       Border setting 
	EPD_4in26_SendData(0x01u);

	EPD_4in26_SendCommand(0x11u);        //    data  entry  mode
	EPD_4in26_SendData(0x01u);           //       X-mode  x+ y-    

	EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT-1, EPD_4in26_WIDTH-1, 0);

	EPD_4in26_SetCursor(0, 0);

	EPD_4in26_ReadBusy();

	//TEMP (1.5s)
	EPD_4in26_SendCommand(0x1Au);  
    EPD_4in26_SendData(0x5Au); 

    EPD_4in26_SendCommand(0x22u);  
    EPD_4in26_SendData(0x91u); 
    EPD_4in26_SendCommand(0x20u); 
	
	EPD_4in26_ReadBusy();
}

void EPD_4in26_Init_4GRAY(void)
{
    configure_pins_and_power_on();
    
    EPD_4in26_Reset();
    LOG_DBG("EPAPER device reset. Init 4GRAY");
	k_msleep(100);

	EPD_4in26_ReadBusy();   
	EPD_4in26_SendCommand(0x12u);  //SWRESET
	EPD_4in26_ReadBusy();   
	
	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);
	EPD_4in26_SendCommand(0x0Cu); //set soft start     
	EPD_4in26_SendData(0xAEu);
	EPD_4in26_SendData(0xC7u);
	EPD_4in26_SendData(0xC3u);
	EPD_4in26_SendData(0xC0u);
	EPD_4in26_SendData(0x80u);
	EPD_4in26_SendCommand(0x01u);   //      drive output control    
	EPD_4in26_SendData((EPD_4in26_WIDTH-1) %256); //  Y  
	EPD_4in26_SendData((EPD_4in26_WIDTH-1) / 256); //  Y 
	EPD_4in26_SendData(0x02u);

	EPD_4in26_SendCommand(0x3Cu);        // Border       Border setting 
	EPD_4in26_SendData(0x01u);

	EPD_4in26_SendCommand(0x11u);        //    data  entry  mode
	EPD_4in26_SendData(0x01u);           //       X-mode  x+ y-    
	EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT-1, EPD_4in26_WIDTH-1, 0);

	EPD_4in26_SetCursor(0, 0);

	EPD_4in26_ReadBusy();

    EPD_4in26_Lut();
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_4in26_Clear(void)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH / 8u;	
	uint8_t image[EPD_4in26_WIDTH / 8u] = {0};
    for(i = 0u; i < width; i++) 
    {
        image[i] = 0xFFu;
    }
    
	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
	    EPD_4in26_SendData2(image, width);
	}

	EPD_4in26_SendCommand(0x26u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2(image, width);
	}
	EPD_4in26_TurnOnDisplay();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void EPD_4in26_Display(uint8_t *Image)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH/8;
	
	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for( i = 0u; i < height; i++)
	{
        EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay();	
}

void EPD_4in26_Display_Base(uint8_t *Image)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH/8;
	
    LOG_DBG("EPD_4in26_Display_Base");
	
    EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}

	EPD_4in26_SendCommand(0x26u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay();	
}

void EPD_4in26_Display_Fast(uint8_t *Image)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH/8;

    LOG_DBG("EPD_4in26_Display_Fast");
	
	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay_Fast();	
}

void EPD_4in26_Display_Part(uint8_t *Image, uint16_t x, uint16_t y, uint16_t w, uint16_t l)
{
	uint16_t i;
	uint16_t height = l;
	uint16_t width =  (w % 8u == 0u) ? (w / 8u): ((w / 8u) + 1);

    LOG_DBG("EPD_4in26_Display_Fast");
    
    EPD_4in26_Reset();

	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x3Cu);        // Border       Border setting 
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SetWindows(x, y, x+w-1, y+l-1);

	EPD_4in26_SetCursor(x, y);

	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for( i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay_Part();	
}

void EPD_4in26_4GrayDisplay(uint8_t *Image)
{
    uint16_t i,j,k;
    uint8_t temp1,temp2,temp3;

    // old  data
    EPD_4in26_SendCommand(0x24);
    for(i = 0u; i < 48000u; i++) 
    {             //5808*4  46464
        temp3=0;
        for(j = 0u; j < 2u; j++) 
        {
            temp1 = Image[i*2+j];
            for(k = 0u; k < 2; k++) 
            {
                temp2 = (temp1 & 0xC0u);
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00u;
                        break;
                    case 0x00u:
                    case 0x80u:
                        temp3 |= 0x01u;
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x01u; 
                        break;
                }
                temp3 <<= 1u;

                temp1 <<= 2u;
                temp2 = (temp1 & 0xC0u);
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00;
                        break;
                    case 0x00u:
                    case 0x80u:
                        temp3 |= 0x01u;
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x00u; 
                        break;
                }

                if((j!=1u) || (k!=1))
                {
                    temp3 <<= 1u;
                }

                temp1 <<= 2u;
            }

        }
        EPD_4in26_SendData(temp3);
        // printf("%x",temp3);
    }

    EPD_4in26_SendCommand(0x26u);   //write RAM for black(0)/white (1)
    for(i = 0u; i < 48000u; i++) 
    {             //5808*4  46464
        temp3=0;
        for( j = 0; j < 2; j++) 
        {
            temp1 = Image[i*2+j];
            for(k=0; k<2; k++) 
            {
                temp2 = temp1 & 0xC0u ;
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00u; //white
                        break;
                    case 0x00u:
                        temp3 |= 0x01u;  //black
                        break;
                    case 0x80u:
                        temp3 |= 0x00u;  //gray1
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x01u; //gray2
                        break;
                }
                temp3 <<= 1u;

                temp1 <<= 2u;
                temp2 = (temp1 & 0xC0u);
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00u;  //white
                        break;
                    case 0x00u:
                        temp3 |= 0x01u; //black
                        break;
                    case 0x80u:
                        temp3 |= 0x00u; //gray1
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x01u;	//gray2
                        break;
                }

                if((j != 1u) || (k != 1u))
                {
                    temp3 <<= 1u;
                }

                temp1 <<= 2u;
            }
        }
        EPD_4in26_SendData(temp3);
        // printf("%x",temp3);
    }

    EPD_4in26_TurnOnDisplay_4GRAY();
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_4in26_Sleep(void)
{
	EPD_4in26_SendCommand(0x10u); //enter deep sleep
	EPD_4in26_SendData(0x03u); 
	k_msleep(100);
    POWER_OFF();
}