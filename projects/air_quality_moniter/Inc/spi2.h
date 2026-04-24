/*
 * spi2.h
 *
 *  Created on: Apr 22, 2026
 *      Author: huray
 */

#ifndef SPI2_H
#define SPI2_H

#include <stdint.h>

/*
 * SPI2 Driver
 * Provides low-level SPI2 communication on PB13 (SCK), PB14 (MISO),
 * PB15 (MOSI), and PB12 (CS as GPIO output).
 * Used to communicate with the BME280 sensor.
 * SPI Mode 0 (CPOL=0, CPHA=0), 1 MHz clock, 8-bit data, MSB first.
 */

/* Configures GPIOB pins, SPI2 peripheral, and enables clocks. */
void spi2_init(void);

/* Sends one byte on MOSI and simultaneously receives one byte on MISO.
 * Returns the byte received from the slave device. */
uint8_t spi2_transmit_receive(uint8_t byte);

/* Pulls PB12 (CS) LOW to select the slave device. */
void spi2_cs_enable(void);

/* Pulls PB12 (CS) HIGH to deselect the slave device. */
void spi2_cs_disable(void);

#endif /* SPI2_H */
