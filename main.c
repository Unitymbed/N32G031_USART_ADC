
#include "n32g031.h"
#include "n32g031_gpio.h"
#include "n32g031_usart.h"
#include "n32g031_adc.h"
#include "n32g031_rcc.h"

#define ADC_FULL_SCALE  4095u
#define VREF_uV         3300000u   // 3.300000 V = 3,300,000 µV

extern uint32_t SystemCoreClock;


static void delay_ms(uint32_t ms)
{
    SysTick->LOAD  = (SystemCoreClock / 1000) - 1;
    SysTick->VAL   = 0;
    SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (ms--) while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    SysTick->CTRL = 0;
}


static void led_init_pb7(void)
{
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    GPIOB->PMODE  &= ~(3U << (7*2));
    GPIOB->PMODE  |=  (1U << (7*2));   // output
    GPIOB->POTYPE &= ~(1U << 7);       // push-pull
    GPIOB->PUPD   &= ~(3U << (7*2));   // no pull
}
static inline void led_toggle_pb7(void){ GPIOB->POD ^= (1U << 7); }


#ifndef GPIO_AF4_USART1
#define GPIO_AF4_USART1 0x04U
#endif

static void usart1_pin_pa9_af4(void)
{
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);

    // PA9 -> Alternate Function mode
    GPIOA->PMODE  &= ~(3U << (9*2));
    GPIOA->PMODE  |=  (2U << (9*2));   // AF
    GPIOA->POTYPE &= ~(1U << 9);       // push-pull
    GPIOA->PUPD   &= ~(3U << (9*2));   // no pull

    // AFH for pin 8..15, PA9 uses bits [7:4]
    GPIOA->AFH    &= ~(0xFU << 4);
    GPIOA->AFH    |=  (GPIO_AF4_USART1 << 4);
}

static void usart1_init_115200(void)
{
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_USART1, ENABLE);

    USART_InitType cfg;
    USART_StructInit(&cfg);            // มีค่า default ให้ก่อน
    cfg.BaudRate   = 9600;
    cfg.WordLength = USART_WL_8B;
    cfg.StopBits   = USART_STPB_1;
    cfg.Parity     = USART_PE_NO;
    cfg.Mode       = USART_MODE_TX;
    cfg.HardwareFlowControl = USART_HFCTRL_NONE;

    USART_Init(USART1, &cfg);          // ไลบรารีคำนวณ BRR จาก PCLK2 ให้อัตโนมัติ
    USART_Enable(USART1, ENABLE);
}

static void usart1_putc(char c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXDE) == RESET) {}
    USART_SendData(USART1, (uint16_t)c);
}
static void usart1_puts(const char* s)
{
    while (*s){
        usart1_putc(*s++);
        led_toggle_pb7();              // กระพริบทุกครั้งที่ส่ง 1 ตัวอักษร
    }
}
static void usart1_putu16(uint16_t v)
{
    char buf[6]; // 0..65535 + '\0'
    int i = 5; buf[i] = '\0';
    do{ buf[--i] = '0' + (v % 10); v /= 10; }while(v);
    usart1_puts(&buf[i]);
}

/*static void usart1_put_voltage_uV(uint32_t uV)
{
    // แปลงเป็น X.YYYY V โดย uV = ไมโครโวลต์
    uint32_t ip = uV / 1000000u;          // ส่วนเต็มเป็นโวลต์
    uint32_t frac4 = (uV % 1000000u) / 100u;  // ทศนิยม 4 ตำแหน่ง (100 µV/step)

    // พิมพ์ส่วนเต็ม
    usart1_putu16((uint16_t)ip);
    usart1_putc('.');

    // พิมพ์ทศนิยม 4 หลัก แบบ zero-pad
    char d3 = '0' + (frac4 / 1000) % 10;
    char d2 = '0' + (frac4 / 100)  % 10;
    char d1 = '0' + (frac4 / 10)   % 10;
    char d0 = '0' + (frac4 / 1)    % 10;
    usart1_putc(d3); usart1_putc(d2); usart1_putc(d1); usart1_putc(d0);

    usart1_puts(" V");
}*/

// แปลงค่า ADC (0..4095) เป็นโวลต์ 4 ตำแหน่งทศนิยม โดย VREF = 3.3V
// ได้ค่าเป็น Vx10000 = V * 10000 (เช่น 3.3000V -> 33000)
static uint32_t adc_to_Vx10000(uint16_t adc)
{
    // +2047 เพื่อปัดเศษ (round) ก่อนหารด้วย 4095
    // 4095*33000 = 135,135,000 ยังอยู่ในช่วง uint32_t
    return ((uint32_t)adc * 33000u + 2047u) / 4095u;
}

static void usart1_put_voltage_4dp(uint16_t adc)
{
    uint32_t v10000 = adc_to_Vx10000(adc); // 0..33000
    uint16_t ip  = (uint16_t)(v10000 / 10000u);  // ส่วนเต็มโวลต์ (0..3)
    uint16_t frac = (uint16_t)(v10000 % 10000u); // ทศนิยม 4 หลัก (0..9999)

    usart1_putu16(ip);
    usart1_putc('.');
    // พิมพ์ทศนิยม 4 หลักแบบ zero-pad
    char d3 = '0' + (frac / 1000) % 10;
    char d2 = '0' + (frac / 100)  % 10;
    char d1 = '0' + (frac / 10)   % 10;
    char d0 = '0' + (frac / 1)    % 10;
    usart1_putc(d3); usart1_putc(d2); usart1_putc(d1); usart1_putc(d0);
    usart1_puts(" V");
}



static void adc_gpio_init_pa0_analog(void)
{
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    // PA0 analog
    GPIOA->PMODE &= ~(3U << (0*2));    // 00b = input/analog
}

static void adc1_init_single_ch0(void)
{
    ADC_InitType adc;

    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);

    ADC_InitStruct(&adc);              // <-- ใช้ชื่อฟังก์ชันแบบที่ชุด N32 ใช้
    adc.MultiChEn       = DISABLE;
    adc.ContinueConvEn  = DISABLE;     // อ่านครั้งต่อครั้ง (จะสั่งเริ่มเอง)
    adc.ExtTrigSelect   = ADC_EXT_TRIGCONV_NONE;
    adc.DatAlign        = ADC_DAT_ALIGN_R;
    adc.ChsNumber       = 1;
    ADC_Init(ADC, &adc);

    ADC_ConfigRegularChannel(ADC, ADC_CH_0, 1, ADC_SAMP_TIME_56CYCLES5);
    ADC_Enable(ADC, ENABLE);
    delay_ms(1);
}

static uint16_t adc1_read_ch0_once(void)
{
    ADC_EnableSoftwareStartConv(ADC, ENABLE);
    while (!ADC_GetFlagStatus(ADC, ADC_FLAG_ENDC));
    ADC_ClearFlag(ADC, ADC_FLAG_ENDC);
    return ADC_GetDat(ADC);
}

int main(void)
{
    SystemInit();             // ตั้ง clock ตาม system_n32g031.c
    SystemCoreClockUpdate();  // sync ค่า SystemCoreClock

    led_init_pb7();
    usart1_pin_pa9_af4();
    usart1_init_115200();

    adc_gpio_init_pa0_analog();
    adc1_init_single_ch0();

    usart1_puts("\r\nN32G031 ADC->USART @115200, CH0=PA0\r\n");

    while (1)
    {
      uint16_t v = adc1_read_ch0_once();
      usart1_puts("ADC=");
      usart1_putu16(v);
      usart1_puts("  V=");
      usart1_put_voltage_4dp(v);
      usart1_puts("\r\n");
      delay_ms(100);
    }
}
