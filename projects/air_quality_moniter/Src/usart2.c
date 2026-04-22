#include "stm32f407xx.h"
#include "usart2.h"

void usart2_init(void)
{
    /* Step 1: Enable clocks
     * GPIOA is on AHB1 bus (bit 0 of AHB1ENR)
     * USART2 is on APB1 bus (bit 17 of APB1ENR) */
    RCC->AHB1ENR |= (1 << 0);
    RCC->APB1ENR |= (1 << 17);

    /* Step 2: Configure PA2 and PA3 as alternate function mode (0b10)
     * MODER fields are 2 bits wide: pin2 = bits [5:4], pin3 = bits [7:6] */
    GPIOA->MODER &= ~(0x3 << 4);   /* Clear PA2 mode bits */
    GPIOA->MODER |=  (0x2 << 4);   /* Set PA2 to alternate function */
    GPIOA->MODER &= ~(0x3 << 6);   /* Clear PA3 mode bits */
    GPIOA->MODER |=  (0x2 << 6);   /* Set PA3 to alternate function */

    /* Step 3: Set alternate function to AF7 (USART2) for PA2 and PA3
     * AFRL fields are 4 bits wide: pin2 = bits [11:8], pin3 = bits [15:12] */
    GPIOA->AFRL &= ~(0xF << 8);    /* Clear PA2 AF bits */
    GPIOA->AFRL |=  (0x7 << 8);    /* Set PA2 to AF7 */
    GPIOA->AFRL &= ~(0xF << 12);   /* Clear PA3 AF bits */
    GPIOA->AFRL |=  (0x7 << 12);   /* Set PA3 to AF7 */

    /* Step 4: Configure USART2 baud rate
     * 16 MHz / (16 * 115200) = 8.6806
     * Mantissa = 8 (0x8), Fraction = 0.6806 * 16 = 11 (0xB)
     * BRR = (8 << 4) | 11 = 0x008B
     * Word length: 8 bits (default, CR1 bit 12 = 0)
     * Stop bits: 1 (default, CR2 bits [13:12] = 00) */
    USART2->BRR = 0x008B;

    /* Step 5: Enable transmitter and USART2 peripheral
     * TE (bit 3): Enables the transmit shift register
     * UE (bit 13): Master enable for the entire USART2 block */
    USART2->CR1 |= (1 << 3) | (1 << 13);
}

/*
 * Sends a single byte over USART2 TX.
 * Blocks (waits in a loop) until the transmit data register is empty,
 * then writes the byte to the data register for transmission.
 */
void usart2_send_byte(uint8_t byte)
{
    /* Wait until TXE (bit 7) in the status register is set.
     * TXE = 1 means the data register is empty and ready for a new byte.
     * TXE = 0 means the previous byte hasn't been moved to the shift
     * register yet, so we must wait. */
    while (!(USART2->SR & (1 << 7)))
    {
        /* Do nothing — just keep checking */
    }

    /* Write the byte to the data register.
     * The hardware automatically moves it to the shift register
     * and begins transmitting out the TX pin (PA2). */
    USART2->DR = byte;
}

/*
 * Sends a null-terminated string over USART2 TX.
 * Loops through each character and sends it one byte at a time.
 * Stops when it reaches the null terminator '\0'.
 */
void usart2_send_string(const char *str)
{
    while (*str != '\0')
    {
        usart2_send_byte((uint8_t)*str);
        str++;
    }
}
