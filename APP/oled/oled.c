#include "oled.h"
#include "delay.h"
#include "oledfont.h"

#define OLED_ADDR  0x78

static void I2C_Start(void)
{
    GPIO_SetBits(OLED_I2C_PORT, OLED_SDA_PIN);
    GPIO_SetBits(OLED_I2C_PORT, OLED_SCL_PIN);
    delay_us(4);
    GPIO_ResetBits(OLED_I2C_PORT, OLED_SDA_PIN);
    delay_us(4);
    GPIO_ResetBits(OLED_I2C_PORT, OLED_SCL_PIN);
}

static void I2C_Stop(void)
{
    GPIO_ResetBits(OLED_I2C_PORT, OLED_SDA_PIN);
    GPIO_SetBits(OLED_I2C_PORT, OLED_SCL_PIN);
    delay_us(4);
    GPIO_SetBits(OLED_I2C_PORT, OLED_SDA_PIN);
    delay_us(4);
}

static uint8_t I2C_WriteByte(uint8_t byte)
{
    uint8_t i, ack;
    for(i = 0; i < 8; i++) {
        if(byte & 0x80)
            GPIO_SetBits(OLED_I2C_PORT, OLED_SDA_PIN);
        else
            GPIO_ResetBits(OLED_I2C_PORT, OLED_SDA_PIN);
        delay_us(2);
        GPIO_SetBits(OLED_I2C_PORT, OLED_SCL_PIN);
        delay_us(2);
        GPIO_ResetBits(OLED_I2C_PORT, OLED_SCL_PIN);
        byte <<= 1;
    }
    GPIO_SetBits(OLED_I2C_PORT, OLED_SDA_PIN);
    GPIO_SetBits(OLED_I2C_PORT, OLED_SCL_PIN);
    delay_us(2);
    ack = GPIO_ReadInputDataBit(OLED_I2C_PORT, OLED_SDA_PIN);
    GPIO_ResetBits(OLED_I2C_PORT, OLED_SCL_PIN);
    return ack;
}

static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR << 1);
    I2C_WriteByte(0x00);
    I2C_WriteByte(cmd);
    I2C_Stop();
}

static void OLED_WriteData(uint8_t data)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR << 1);
    I2C_WriteByte(0x40);
    I2C_WriteByte(data);
    I2C_Stop();
}

void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(OLED_I2C_PORT, &GPIO_InitStructure);
    
    delay_ms(100);
    
    OLED_WriteCmd(0xAE); // display off
    OLED_WriteCmd(0x00); // set lower column address
    OLED_WriteCmd(0x10); // set higher column address
    OLED_WriteCmd(0x40); // set display start line
    OLED_WriteCmd(0xB0); // set page address
    OLED_WriteCmd(0x81); // set contrast
    OLED_WriteCmd(0xFF);
    OLED_WriteCmd(0xA1); // set segment remap
    OLED_WriteCmd(0xA6); // normal display
    OLED_WriteCmd(0xA8); // set multiplex ratio
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xC8); // COM scan direction
    OLED_WriteCmd(0xD3); // set display offset
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xD5); // set osc frequency
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xD9); // set pre-charge period
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDA); // set COM pins
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0xDB); // set vcomh
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x8D); // charge pump enable
    OLED_WriteCmd(0x14);
    OLED_WriteCmd(0xAF); // display on
    
    OLED_Clear();
}

void OLED_Clear(void)
{
    uint8_t i, j;
    for(i = 0; i < 8; i++) {
        OLED_WriteCmd(0xB0 + i);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);
        for(j = 0; j < 128; j++) {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size)
{
    uint8_t i;
    uint8_t *pFont;
    
    if(size == 8) {
        pFont = (uint8_t *)ASCII_6x8[chr - 32];
        for(i = 0; i < 8; i++) {
            OLED_WriteCmd(0xB0 + y);
            OLED_WriteCmd(((x + i) & 0x0F) | 0x00);
            OLED_WriteCmd(0x10 | ((x + i) >> 4));
            OLED_WriteData(pFont[i]);
        }
    } else if(size == 16) {
        pFont = (uint8_t *)ASCII_8x16[chr - 32];
        for(i = 0; i < 16; i++) {
            OLED_WriteCmd(0xB0 + y + (i/8));
            OLED_WriteCmd(((x + (i%8)) & 0x0F) | 0x00);
            OLED_WriteCmd(0x10 | ((x + (i%8)) >> 4));
            OLED_WriteData(pFont[i]);
        }
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    while(*str) {
        OLED_ShowChar(x, y, *str, size);
        x += size / 2;
        if(x > 120) { x = 0; y += 2; }
        str++;
    }
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    char buf[12];
    uint8_t i;
    for(i = 0; i < len; i++) buf[i] = ' ';
    buf[len] = '\0';
    i = len - 1;
    do {
        buf[i--] = num % 10 + '0';
        num /= 10;
    } while(num && i != 0xFF);
    OLED_ShowString(x, y, buf, size);
}
