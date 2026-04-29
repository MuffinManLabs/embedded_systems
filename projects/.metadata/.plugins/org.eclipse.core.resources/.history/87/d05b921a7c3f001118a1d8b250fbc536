#ifndef USART2_H
#define USART2_H

#include <stdint.h>

/*
 * USART2 Driver
 * Provides basic transmit functionality over USART2 (PA2 TX, PA3 RX).
 * Used as the debug serial output for the air quality monitor.
 * Baud rate: 115200, 8 data bits, 1 stop bit, no parity.
 */

/* Configures GPIOA pins, USART2 peripheral, and enables clocks. */
void usart2_init(void);

/* Sends a single byte out USART2 TX. Blocks until transmit register is empty. */
void usart2_send_byte(uint8_t byte);

/* Sends a null-terminated string out USART2 TX, one byte at a time. */
void usart2_send_string(const char *str);

#endif /* USART2_H */
