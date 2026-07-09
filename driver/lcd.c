#include "stm32f4xx.h"                  // Device header
#include <string.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "delay.h"
#include "lcd.h"
#include "font.h"
#include "image.h"

#define LCD_DMA_STREAM        DMA1_Stream4
#define LCD_DMA_CHANNEL       DMA_Channel_0
#define LCD_DMA_TC_FLAG       DMA_FLAG_TCIF4
#define LCD_DMA_ALL_FLAGS     (DMA_FLAG_FEIF4 | DMA_FLAG_DMEIF4 | DMA_FLAG_TEIF4 | DMA_FLAG_HTIF4 | DMA_FLAG_TCIF4)
#define LCD_DMA_CHUNK_PIXELS  1024

static uint16_t lcd_dma_buffer[LCD_DMA_CHUNK_PIXELS];
static SemaphoreHandle_t lcd_dma_semaphore;

static void Lcd_Dma_Init(void);
static void Lcd_Dma_Int_Init(void);
static void LCD_WriteGram16_DMA(const uint16_t *data, uint32_t pixels, bool memory_inc);
static void LCD_WriteColor_DMA(uint16_t color, uint32_t pixels);
static void LCD_WriteImage_DMA(const uint8_t *data, uint32_t pixels);

/*开始初始化液晶屏LCD，型号为ST7789*/
static void Spi2_Init(void)
{	
    GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);
    // 2. 初始化复用引脚 (PB13 SCK, PB15 MOSI)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;          // 复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;        // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;      // 高速
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
	
    // 3. 映射复用功能到 SPI2
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);

    // 4. 初始化控制引脚 (PD10 DC, PD11 RST, PD12 CS)
    GPIO_InitStructure.GPIO_Pin = LCD_DC_Pin | LCD_RST_Pin | LCD_CS_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;         // 普通推挽输出
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;          // 默认上拉
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // 5. 提前拉高 CS，防止屏幕上电时误听指令
    LCD_CS_High();

    // 6. 硬件 SPI2 参数配置
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; // 全双工
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                      // 主机模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                  // 8位数据
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;                         // 模式0：空闲低电平
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;                       // 模式0：第一个边沿采样
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                          // 软件控制 CS
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; //4分频，10.4MHZ
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;                 // 高位在前
    SPI_Init(SPI2, &SPI_InitStructure);
	
    SPI_Cmd(SPI2, ENABLE);
    Lcd_Dma_Init();
}

static void Lcd_Dma_Int_Init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(DMA1_Stream4_IRQn, 5);
}
static void Lcd_Dma_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    lcd_dma_semaphore = xSemaphoreCreateBinary();
    configASSERT(lcd_dma_semaphore);
    DMA_Cmd(LCD_DMA_STREAM, DISABLE);
    while (DMA_GetCmdStatus(LCD_DMA_STREAM) != DISABLE);
    DMA_DeInit(LCD_DMA_STREAM);
    DMA_ClearFlag(LCD_DMA_STREAM, LCD_DMA_ALL_FLAGS);

    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_Channel = LCD_DMA_CHANNEL;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)lcd_dma_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize = 1;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_ITConfig(LCD_DMA_STREAM, DMA_IT_TC, ENABLE);
    DMA_Init(LCD_DMA_STREAM, &DMA_InitStructure);
    Lcd_Dma_Int_Init();

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
}

static void LCD_WriteGram16_DMA(const uint16_t *data, uint32_t pixels, bool memory_inc)
{
    while (pixels > 0)
    {
        uint16_t chunk = (pixels > 65535U) ? 65535U : (uint16_t)pixels;

        DMA_Cmd(LCD_DMA_STREAM, DISABLE);
        while (DMA_GetCmdStatus(LCD_DMA_STREAM) != DISABLE);
        DMA_ClearFlag(LCD_DMA_STREAM, LCD_DMA_ALL_FLAGS);

        SPI_DataSizeConfig(SPI2, SPI_DataSize_16b);
        if (memory_inc)
            LCD_DMA_STREAM->CR |= DMA_SxCR_MINC;
        else
            LCD_DMA_STREAM->CR &= ~DMA_SxCR_MINC;

        LCD_DMA_STREAM->M0AR = (uint32_t)data;
        LCD_DMA_STREAM->NDTR = chunk;
        DMA_Cmd(LCD_DMA_STREAM, ENABLE);

        xSemaphoreTake(lcd_dma_semaphore, portMAX_DELAY);
        while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET);
        (void)SPI_I2S_ReceiveData(SPI2);
        (void)SPI2->SR;
        SPI_DataSizeConfig(SPI2, SPI_DataSize_8b);

        if (memory_inc)
            data += chunk;
        pixels -= chunk;
    }
}

static void LCD_WriteColor_DMA(uint16_t color, uint32_t pixels)
{
    LCD_WriteGram16_DMA(&color, pixels, false);
}

static void LCD_WriteImage_DMA(const uint8_t *data, uint32_t pixels)
{
    while (pixels > 0)
    {
        uint16_t chunk = (pixels > LCD_DMA_CHUNK_PIXELS) ? LCD_DMA_CHUNK_PIXELS : (uint16_t)pixels;
        uint16_t i;

        for (i = 0; i < chunk; i++)
        {
            lcd_dma_buffer[i] = ((uint16_t)data[0] << 8) | data[1];
            data += 2;
        }

        LCD_WriteGram16_DMA(lcd_dma_buffer, chunk, true);
        pixels -= chunk;
    }
}
/* 硬件发送函数 */
static void SPI_WriteByte(uint8_t data)
{
    // 等待发送缓冲区空
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET); 
    // 填入数据
    SPI_I2S_SendData(SPI2, data);	
    // 等待接收缓冲区收到伪数据 (标志着这一帧彻底在引脚上跑完了)
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET); 
    // 读出伪数据，清空 RXNE 标志位，防止溢出错误 (OVR)
    SPI_I2S_ReceiveData(SPI2);		
}

static void LCD_Reset(void)	//复位
{
	LCD_RST_Clr();			//DC置0,开始复位
	cpu_delay_ms(20);		//至少等10ms
	LCD_RST_Set();		//DC置1,唤醒成功开始工作
	cpu_delay_ms(120);
}

static void LCD_WriteCmd(uint8_t cmd)	//向液晶屏写命令
{
	LCD_DC_Cmd();			//告诉lcd这是命令
	LCD_CS_Low();			//选中，开始接收数据
	SPI_WriteByte(cmd);
	LCD_CS_High();		//释放CS
}

static void LCD_WriteData(uint8_t data)  //向液晶屏写数据
{
	LCD_DC_Data();			//告诉lcd这是数据
	LCD_CS_Low();
	SPI_WriteByte(data);
	LCD_CS_High();
}

void LCD_Init(void) 
{
	Spi2_Init();
    // 1. 唤醒与复位 
    LCD_Reset(); // 里面已经包含了拉低、拉高和 cpu_delay_ms(120)

    // 2. 开始灌入 ST7789V 的灵魂 (直接照搬厂家的核心参数)
    LCD_WriteCmd(0x3A);        // 颜色格式设置 (65k mode)
    LCD_WriteData(0x05);       // 0x05 表示 16-bit/pixel (RGB565格式)

    LCD_WriteCmd(0xC5);        // VCOM 电压设置 (影响屏幕对比度)
    LCD_WriteData(0x1A);

    LCD_WriteCmd(0x36);        // 屏幕显示方向设置 (非常重要！)
    LCD_WriteData(0x00);       // 0x00 默认方向。如果屏幕画面倒了，以后改这个值

    // ------------- Frame rate setting (帧率设置) -----------
    LCD_WriteCmd(0xB2);        // Porch Setting
    LCD_WriteData(0x05);
    LCD_WriteData(0x05);
    LCD_WriteData(0x00);
    LCD_WriteData(0x33);
    LCD_WriteData(0x33);

    LCD_WriteCmd(0xB7);        // Gate Control
    LCD_WriteData(0x05);

    // -------------- Power setting (电源电压设置) ---------------
    LCD_WriteCmd(0xBB);        // VCOM
    LCD_WriteData(0x3F);

    LCD_WriteCmd(0xC0);        // Power control
    LCD_WriteData(0x2C);

    LCD_WriteCmd(0xC2);        // VDV and VRH Command Enable
    LCD_WriteData(0x01);

    LCD_WriteCmd(0xC3);        // VRH Set
    LCD_WriteData(0x0F);

    LCD_WriteCmd(0xC4);        // VDV Set
    LCD_WriteData(0x20);

    LCD_WriteCmd(0xC6);        // Frame Rate Control
    LCD_WriteData(0x01);       // 111Hz 刷新率

    LCD_WriteCmd(0xD0);        // Power Control 1
    LCD_WriteData(0xA4);
    LCD_WriteData(0xA1);

    LCD_WriteCmd(0xE8);        // Power Control 2
    LCD_WriteData(0x03);

    LCD_WriteCmd(0xE9);        // Equalize time control
    LCD_WriteData(0x09);
    LCD_WriteData(0x09);
    LCD_WriteData(0x08);

    // --------------- Gamma setting (伽马色彩校正，直接抄不用管) -------------
    LCD_WriteCmd(0xE0); 
    LCD_WriteData(0xD0); LCD_WriteData(0x05); LCD_WriteData(0x09); LCD_WriteData(0x09);
    LCD_WriteData(0x08); LCD_WriteData(0x14); LCD_WriteData(0x28); LCD_WriteData(0x33);
    LCD_WriteData(0x3F); LCD_WriteData(0x07); LCD_WriteData(0x13); LCD_WriteData(0x14);
    LCD_WriteData(0x28); LCD_WriteData(0x30);
     
    LCD_WriteCmd(0xE1); 
    LCD_WriteData(0xD0); LCD_WriteData(0x05); LCD_WriteData(0x09); LCD_WriteData(0x09);
    LCD_WriteData(0x08); LCD_WriteData(0x03); LCD_WriteData(0x24); LCD_WriteData(0x32);
    LCD_WriteData(0x32); LCD_WriteData(0x3B); LCD_WriteData(0x14); LCD_WriteData(0x13);
    LCD_WriteData(0x28); LCD_WriteData(0x2F);

    LCD_WriteCmd(0x20);        // 反显指令 (有些厂家的屏颜色是反的，靠这个修正)

    //  最后的点亮仪式
    LCD_WriteCmd(0x11);        // Exit Sleep (退出睡眠模式)
    cpu_delay_ms(120);             // 强调过的“起床时间”
    
    LCD_WriteCmd(0x29);        // Display ON (正式开启显示！)
}

// ---  设置显示窗口 (告诉屏幕你要涂哪一块) ---
static void LCD_AddressSet(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    LCD_WriteCmd(0x2a); // 列地址设置
    LCD_WriteData(x1 >> 8); LCD_WriteData(x1);
    LCD_WriteData(x2 >> 8); LCD_WriteData(x2);

    LCD_WriteCmd(0x2b); // 行地址设置
    LCD_WriteData(y1 >> 8); LCD_WriteData(y1);
    LCD_WriteData(y2 >> 8); LCD_WriteData(y2);

    LCD_WriteCmd(0x2c); // 发送这个指令后，接下来发的数据全会写进显存
}

//填色函数，参数为--起始坐标(x1,y1),结束坐标(x2,y2),颜色color
void LCD_Fill_Color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint32_t pix = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

    LCD_AddressSet(x1, y1, x2, y2);
    LCD_DC_Data();
    LCD_CS_Low();
    LCD_WriteColor_DMA(color, pix);
    LCD_CS_High();
}//清屏函数
void LCD_Clear(uint16_t color)
{
	LCD_Fill_Color(0,0,LCD_WIDTH-1,LCD_HEIGHT-1,color);
}

//定义一个印刷显示函数，只管刷东西
static void LCD_draw_font(uint16_t x, uint16_t y, uint16_t fwidth, uint16_t fheight, const uint8_t *model, uint16_t fc, uint16_t bc)
{
    uint16_t row, col, byte_idx;
    uint8_t temp;
    uint32_t out = 0;
    uint16_t bytes_per_row = (fwidth + 7) / 8;

    LCD_AddressSet(x, y, x + fwidth - 1, y + fheight - 1);
    LCD_DC_Data();
    LCD_CS_Low();

    for (row = 0; row < fheight; row++)
    {
        for (byte_idx = 0; byte_idx < bytes_per_row; byte_idx++)
        {
            temp = model[row * bytes_per_row + byte_idx];
            for (col = 0; col < 8; col++)
            {
                if ((byte_idx * 8 + col) < fwidth)
                {
                    lcd_dma_buffer[out++] = (temp & 0x80) ? fc : bc;
                    if (out >= LCD_DMA_CHUNK_PIXELS)
                    {
                        LCD_WriteGram16_DMA(lcd_dma_buffer, out, true);
                        out = 0;
                    }
                    temp <<= 1;
                }
            }
        }
    }

    if (out > 0)
        LCD_WriteGram16_DMA(lcd_dma_buffer, out, true);

    LCD_CS_High();
}
static const uint8_t *ascii_get_model(const char ch, const font_t *font, uint16_t bytes_per_char)
{
    // 如果人家提供了专门的映射字典（比如搬过来的 54 号 Maple 字库）
    if (font->ascii_map)
    {
        const char *map = font->ascii_map;
        uint32_t idx = 0;
        while (map[idx] != '\0')
        {
            if (map[idx] == ch)
            {
                // 精准定位：在字典里是第几个，就乘以单字总体积，绝对不会指歪！
                return font->ascii_model + (idx * bytes_per_char);
            }
            idx++;
        }
        return NULL; // 如果字符串里找不到这个字符，直接拒绝印刷，防止乱码
    }
    else
    {
        // 如果是你自己手动取模的传统全表字库，依然用传统的 ASCII 码减空格偏移公式
        return font->ascii_model + (ch - ' ') * bytes_per_char;
    }
}

//提取ASCII码的字符，交给印刷函数
// 提取ASCII码的字符，交给印刷函数
static void LCD_write_ascii(uint16_t x, uint16_t y, char ch, uint16_t fc, uint16_t bc, const font_t *font)
{
    if(font == NULL) return;
    if(ch < ' ' || ch > '~') return;        // 字符合格性检查
    
    uint16_t fheight = font->size;          // 字高
    uint16_t fwidth;                        // 窗口宽度
    uint16_t bytes_per_row;                 // 每一行占多少字节

    // ==================== ? 严格对齐各尺寸字模的真实物理体积 ====================
    if (font->size == 32) 
    {
        bytes_per_row = 2;  // 32号字：标准 64 字节（每行 2 字节）
        fwidth = 16;        
    } 
    else if (font->size == 54) 
    {
        bytes_per_row = 4;  // 54号字：标准 216 字节（每行 4 字节）
        fwidth = 27;        
    } 
    else if (font->size == 76) 
    {
        // ?【核心修复】：根据你贴出的 380 字节数据，76 号字每行严格占 5 字节（占阵40）！
        bytes_per_row = 5;  
        fwidth = 38;        // 字符实际物理印刷宽度保持 38 像素
    }
    else 
    {
        // 24号字等小字号保持默认老规矩
        fwidth = font->size / 2;
        bytes_per_row = (fwidth + 7) / 8;
    }
    // ============================================================================
    
    // 计算当前字高下，一个字符包含的物理总内存字节数 (76号字此处精准等于 76 * 5 = 380字节)
    uint16_t bytes_per_char = fheight * bytes_per_row;
    
    // 调用字典指针定位器，精准抓取字模
    const uint8_t *model = ascii_get_model(ch, font, bytes_per_char);
    
    if(model != NULL)
    {
        LCD_draw_font(x, y, fwidth, fheight, model, fc, bc);    // 交付硬件窗口印刷
    }
}

//中文
static void LCD_write_chinese(uint16_t x, uint16_t y, char *ch, uint16_t fc, uint16_t bc, const font_t *font)
{
	//防止传了空的字符串指针,防止你忘记传font,防止你忘记把中文给进font
	if(ch == NULL || font == NULL || font->chinese == NULL) return ;
	
	uint16_t fwideth = font->size;
	uint16_t fheight = font->size;		//中文正常
	
	const font_chinese_t *c = font->chinese ;
	
	for(; c->name != NULL ; c ++)
	{
		if(strcmp(c->name, ch) == 0)	//让输入的ch与chinese_font[]结构体里的name对比，对比一致输出0
		{
			LCD_draw_font(x,y,fwideth,fheight,c->model,fc,bc);
			return;
		}
	}
}
//检测是否印刷到中文
static bool is_gb2312(char ch)
{
    // GB2312 编码的汉字，第一个字节总是大于 0xA0
    return ( (unsigned char) ch >= 0xA1); 
}

void LCD_Show_String(uint16_t x, uint16_t y, const char *str, uint16_t fc, uint16_t bc, const font_t *font)
{
    uint16_t start_x = x;     // 记录换行的初始点位
    
    while(*str != '\0')
    {    
        // 1.检测是不是中文
        int len = is_gb2312(*str) ? 2 : 1; 
        
        // 2.计算当前字符的实际物理视窗宽度 (用于精确边界检测和步进)
        uint16_t current_width;
        if (len == 1)
        {
            // 英文路线宽度计算
            if (font->size == 32)      current_width = 16; // 32号字实际宽 16
            else if (font->size == 54) current_width = 27; // 54号字实际宽 27
            else if (font->size == 76) current_width = 38; // 76号字实际宽 38
            else                       current_width = font->size / 2; // 普通小字号规则
        }
        else
        {
            // 中文路线：汉字是个正方形，宽严格等于字高
            current_width = font->size;
        }
        
        // 换行边界检测
        if(x + current_width > LCD_WIDTH)
        {
            x = start_x;       // 回到初始点
            y += font->size;   // 换行
            
            if(*str == ' ' && len == 1) 
            {
                str++;
                continue;
            }
        }
        if(y + font->size > LCD_HEIGHT) break;
        
        // 3.看菜吃饭，分流渲染
        if(len == 1)
        {
            LCD_write_ascii(x, y, *str, fc, bc, font);
            str++;  
            
            x += current_width; 
        }
        else 
        {
            // ---------------- 中文路线 ----------------
            char ch[5] = {0};        
            strncpy(ch, str, len);   
            
            LCD_write_chinese(x, y, ch, fc, bc, font);
            str += len;              
            x += font->size;         
        }
    }
}

void LCD_Show_Photo(uint16_t x, uint16_t y, const image_t *image)
{
    uint32_t pixels;

    if (image == NULL || image->data == NULL) return;
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT ||
        x + image->width - 1 >= LCD_WIDTH || y + image->height - 1 >= LCD_HEIGHT) return;

    pixels = (uint32_t)image->width * image->height;

    LCD_AddressSet(x, y, x + image->width - 1, y + image->height - 1);
    LCD_DC_Data();
    LCD_CS_Low();
    LCD_WriteImage_DMA(image->data, pixels);
    LCD_CS_High();
}

void DMA1_Stream4_IRQHandler(void)
{
    if (DMA_GetITStatus(LCD_DMA_STREAM, DMA_IT_TCIF4) == SET)
    {
        BaseType_t higher_priority_task_woken = pdFALSE;

        DMA_ClearITPendingBit(LCD_DMA_STREAM, DMA_IT_TCIF4);
        xSemaphoreGiveFromISR(lcd_dma_semaphore, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}
