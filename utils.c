#include "n32g031.h"

void USART1_SendString(const char* str) {
    while (*str) {
        while (!(USART1->STS & USART_FLAG_TXDE));
        USART1->DAT = *str++;
    }
}

void delay_ms(uint32_t ms) {
    volatile uint32_t count;
    while (ms--) {
        count = 48000;  // ถ้าใช้ 48 MHz
        while (count--) __NOP();
    }
}

