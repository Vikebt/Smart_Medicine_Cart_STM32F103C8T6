#include "hx711.h"
#include "delay.h"   // 假设有一个微秒延时函数 delay_us()

static long g_offset = 0;      // 去皮偏移量
static float g_scale = 1.0f;   // 比例系数

void HX711_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    // SCK 推挽输出
    GPIO_InitStructure.GPIO_Pin = HX711_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HX711_SCK_PORT, &GPIO_InitStructure);
    
    // DOUT 浮空输入
    GPIO_InitStructure.GPIO_Pin = HX711_DOUT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(HX711_DOUT_PORT, &GPIO_InitStructure);
    
    HX711_SCK_LOW();  // 初始低电平
}

// 读取一次原始值（24位有符号数）
uint32_t HX711_Read(void)
{
    uint32_t val = 0;
    uint8_t i;
    
    // 等待 DOUT 变低，表示数据准备好
    while(HX711_DOUT_READ() == SET);
    
    // 读取24位数据，高位在前
    for(i = 0; i < 24; i++)
    {
        HX711_SCK_HIGH();
        delay_us(1);  // 至少1us高电平
        val <<= 1;
        if(HX711_DOUT_READ() == SET) val |= 1;
        HX711_SCK_LOW();
        delay_us(1);
    }
    
    // 第25个脉冲，选择通道A增益128（下次转换）
    HX711_SCK_HIGH();
    delay_us(1);
    HX711_SCK_LOW();
    delay_us(1);
    
    // 24位有符号数扩展为32位有符号数
    if(val & 0x800000) val |= 0xFF000000;
    return (int32_t)val;
}

// 读取多次取平均值（提高稳定性）
static uint32_t HX711_Read_Average(uint8_t times)
{
    uint32_t sum = 0;
    for(uint8_t i = 0; i < times; i++)
        sum += HX711_Read();
    return sum / times;
}

// 获取重量（单位：克）
float HX711_Read_Weight(void)
{
    int32_t raw = (int32_t)HX711_Read_Average(10);
    return (raw - g_offset) / g_scale;
}

// 设置偏移量（用于去皮）
void HX711_Set_Offset(long offset)
{
    g_offset = offset;
}

// 设置比例系数（需根据实际传感器标定）
void HX711_Set_Scale(float scale)
{
    g_scale = scale;
}

// 去皮：将当前读数作为零点
void HX711_Tare(void)
{
    g_offset = (int32_t)HX711_Read_Average(20);
}
