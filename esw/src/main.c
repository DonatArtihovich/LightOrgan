#include "ch32fun.h"
#include "stdarg.h"
#include "stdio.h"

#define UART_LOG_EN (0)

void uart_print(const char *str);
void uart_send_midi_packet(uint16_t *midi_data, uint8_t len);
void init_uart();
void init_adc();
uint32_t adc_read(uint8_t ch);

#define MIDI_SOP (0xAEAE) // Start of packet marker
#define MIDI_EOP (0xEBEB) // End of packet marker

uint8_t adc_channels[] = {0, 1, 2, 3, 4, 6, 7};

int main()
{
    SystemInit();

    init_uart();
    init_adc();

    uint16_t midi_buf[sizeof(adc_channels) / sizeof(*adc_channels)] = {0};
#if defined(UART_LOG_EN) && UART_LOG_EN
    char message[128] = {0};
#endif

    while (1)
    {
#if defined(UART_LOG_EN) && UART_LOG_EN
        uart_print("==============\n\r");
#endif
        for (int i = 0; i < sizeof(adc_channels) / sizeof(*adc_channels); i++)
        {
            midi_buf[i] = adc_read(adc_channels[i]);
#if defined(UART_LOG_EN) && UART_LOG_EN
            snprintf(message, sizeof(message), "Read ADC%d: %d\n\r", i, midi_buf[i]);
            // CH0 - PA2
            // CH1 - PA1
            // CH2 - PC4
            // CH3 - PD2
            // CH4 - PD3
            // CH5 - PD5
            // CH6 - PD6
            // CH7 - PD4
            uart_print(message);
            memset(message, 0, sizeof(message));
#endif
        }
#if defined(UART_LOG_EN) && UART_LOG_EN
        uart_print("==============\n\r");
#endif
        uart_send_midi_packet(midi_buf, sizeof(midi_buf) / sizeof(*midi_buf));
        Delay_Ms(800);
    }
}

void init_uart(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1;

    GPIOD->CFGLR &= ~(0xF << (5 * 4));
    GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (5 * 4);

    USART1->BRR = 24000000 * 2 / 115200;

    USART1->CTLR1 = USART_CTLR1_TE | USART_CTLR1_UE;
}

void uart_sendbyte(uint8_t c) {
    while (!(USART1->STATR & USART_STATR_TXE));
    USART1->DATAR = c;
}

void uart_send_u16(uint16_t dword)
{
    uart_sendbyte(dword >> 8);
    uart_sendbyte(dword & 0xFF);
}

void uart_send_midi_packet(uint16_t *midi_data, uint8_t len)
{
#if defined(UART_LOG_EN) && UART_LOG_EN
    char msg[100] = {0};
    snprintf(msg, sizeof(msg), "Sending MIDI packet with %d values\n\r", len);
    uart_print(msg);
#endif
    uart_send_u16(MIDI_SOP);

    for (int i = 0; i < len; i++)
        uart_send_u16(midi_data[i]);

    uart_send_u16(MIDI_EOP);
}

void uart_print(const char *str) {
    while (*str) {
        uart_sendbyte(*str++);
    }
}

void init_adc() {
    RCC->APB2PCENR |= RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC;

    GPIOA->CFGLR &= ~((0xF << (2 * 4)) | (0xF << (1 * 4))); // A2, A1
    GPIOC->CFGLR &= ~(0xF << (4 * 4)); // C4
    GPIOD->CFGLR &= ~(0xF0FFF << (2 * 4)); // D2 - D4, D6

    RCC->CFGR0 &= ~RCC_ADCPRE;
    RCC->CFGR0 |= RCC_ADCPRE_DIV6;

    ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;

    ADC1->CTLR2 |= ADC_RSTCAL;
    while(ADC1->CTLR2 & ADC_RSTCAL);
    ADC1->CTLR2 |= ADC_CAL;
    while(ADC1->CTLR2 & ADC_CAL);
}

uint32_t adc_read(uint8_t ch)
{
    ADC1->RSQR1 &= ~ADC_L;

    ADC1->RSQR3 = (ADC1->RSQR3 & ~ADC_SQ1) | ch;

    ADC1->CTLR2 |= ADC_SWSTART;
    while(!(ADC1->STATR & ADC_EOC));

    return ADC1->RDATAR;
}

// void init_gpio()
// {
//     RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;

//     GPIOD->CFGLR &= ~(0xF << 16);
//     GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << 16;
// }

// void gpio_set(GPIO_TypeDef *port, uint8_t pin, uint8_t level)
// {
//     level &= 0x1;

//     if (level)
//     {
//         port->BSHR |= (1 << pin);
//     } else {
//         port->BSHR |= (1 << (16 + pin));
//     }
// }