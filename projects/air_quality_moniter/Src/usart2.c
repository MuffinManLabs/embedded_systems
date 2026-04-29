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




void usart2_send_hex(uint32_t value)
{
    /* Print the literal "0x" prefix so output is unambiguous. */
    usart2_send_byte('0');
    usart2_send_byte('x');

    /* A 32-bit value has 8 nibbles (4 bits each). We print from the
     * most-significant nibble down to the least-significant nibble
     * so the output reads left-to-right like a normal hex number.
     *
     * Loop invariant: `shift` holds the bit position of the nibble
     * we're about to extract, starting at bit 28 and decreasing by 4
     * each iteration until we process bit 0. */
    for (int32_t shift = 28; shift >= 0; shift -= 4)
    {
        /* Isolate the current 4-bit nibble:
         *   value >> shift  moves the target nibble down to bits [3:0]
         *   & 0x0F          masks off everything above bits [3:0]
         * Result is a value between 0 and 15 inclusive. */
        uint8_t nibble = (uint8_t)((value >> shift) & 0x0FU);

        /* Convert the nibble to an ASCII hex character.
         * Nibbles 0–9  become '0'–'9' (ASCII 0x30–0x39).
         * Nibbles 10–15 become 'A'–'F' (ASCII 0x41–0x46). */
        uint8_t ch;
        if (nibble < 10U)
        {
            ch = (uint8_t)('0' + nibble);
        }
        else
        {
            ch = (uint8_t)('A' + (nibble - 10U));
        }

        usart2_send_byte(ch);
    }
}


/*
 * Sends a signed 32-bit integer as ASCII decimal digits over USART2.
 *
 * Algorithm:
 *   1. If negative, print '-' and convert to unsigned absolute value.
 *      (The cast through uint32_t handles INT32_MIN correctly — its
 *       positive twin doesn't fit in int32_t, but the unsigned bit
 *       pattern wraps cleanly.)
 *   2. Special-case 0 (the digit-extraction loop would print nothing).
 *   3. Repeatedly take the last digit (n % 10), store as ASCII in a
 *      buffer, then divide by 10. Digits come out in reverse order.
 *   4. Print the buffer back-to-front so the output reads left-to-right.
 *
 * Buffer size: a 32-bit value is at most 10 decimal digits, plus a
 * possible sign. 12 bytes leaves safe headroom.
 */
void usart2_send_int(int32_t value)
{
    char buf[12];
    int32_t idx = 0;
    uint32_t abs_val;

    if (value < 0)
    {
        usart2_send_byte('-');
        /* Negate via unsigned to avoid signed overflow on INT32_MIN. */
        abs_val = -(uint32_t)value;
    }
    else
    {
        abs_val = (uint32_t)value;
    }

    /* Special case: 0 would skip the digit-extraction loop entirely. */
    if (abs_val == 0U)
    {
        usart2_send_byte('0');
        return;
    }

    /* Extract digits in reverse order (least significant first). */
    while (abs_val > 0U)
    {
        buf[idx] = (char)('0' + (abs_val % 10U));
        idx++;
        abs_val /= 10U;
    }

    /* Print the buffer in reverse so the most-significant digit prints first. */
    while (idx > 0)
    {
        idx--;
        usart2_send_byte((uint8_t)buf[idx]);
    }
}
