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


/* Sends a 32-bit unsigned value as ASCII hex over USART2, prefixed with "0x".
 * Always prints 8 hex digits (zero-padded) so values line up in columns when
 * multiple are printed back-to-back. Does not append a newline.
 * Example: usart2_send_hex(0x6B70) prints "0x00006B70". */
void usart2_send_hex(uint32_t value);


/* Sends a signed 32-bit integer as ASCII decimal digits over USART2.
 * Handles negative values by printing a leading '-'. Does not append
 * a newline or any padding.
 * Example: usart2_send_int(-2345) prints "-2345". */
void usart2_send_int(int32_t value);

#endif /* USART2_H */
