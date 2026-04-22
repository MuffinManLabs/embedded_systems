#include "stm32f407xx.h"
#include "spi2.h"


void spi2_init(void)
{
    /* Step 1: Enable clocks
     * GPIOB is on AHB1 bus (bit 1 of AHB1ENR)
     * SPI2 is on APB1 bus (bit 14 of APB1ENR) */
    RCC->AHB1ENR |= (1 << 1);
    RCC->APB1ENR |= (1 << 14);

    /* Step 2: Configure PB13, PB14, PB15 as alternate function mode (0b10)
     * MODER fields are 2 bits wide:
     * pin12 = bits [25:24], pin13 = bits [27:26]
     * pin14 = bits [29:28], pin15 = bits [31:30] */
    GPIOB->MODER &= ~(0xFF << 24);   /* Clear mode bits for PB12-PB15 */
    GPIOB->MODER |=  (0x1 << 24);    /* PB12 = general purpose output (0b01) for CS */
    GPIOB->MODER |=  (0x2 << 26);    /* PB13 = alternate function (0b10) for SCK */
    GPIOB->MODER |=  (0x2 << 28);    /* PB14 = alternate function (0b10) for MISO */
    GPIOB->MODER |=  (0x2 << 30);    /* PB15 = alternate function (0b10) for MOSI */

    /* Step 3: Set alternate function to AF5 (SPI2) for PB13, PB14, PB15
     * AFRH fields are 4 bits wide (for pins 8-15):
     * pin13 = bits [23:20], pin14 = bits [27:24], pin15 = bits [31:28]
     * PB12 is GPIO output so it does not need an alternate function. */
    GPIOB->AFRH &= ~(0xFFF << 20);   /* Clear AF bits for PB13, PB14, PB15 */
    GPIOB->AFRH |=  (0x5 << 20);     /* PB13 = AF5 (SPI2_SCK) */
    GPIOB->AFRH |=  (0x5 << 24);     /* PB14 = AF5 (SPI2_MISO) */
    GPIOB->AFRH |=  (0x5 << 28);     /* PB15 = AF5 (SPI2_MOSI) */

    /* Step 4: Set CS (PB12) HIGH to deselect the sensor at startup.
     * CS is active low, so HIGH = sensor not selected. */
    GPIOB->ODR |= (1 << 12);

    /* Step 5: Configure SPI2 peripheral in SPI_CR1 register
     * MSTR (bit 2):   Master mode — STM32 generates the clock
     * SSM  (bit 9):   Software slave management — we handle CS via GPIO
     * SSI  (bit 8):   Internal slave select HIGH — prevents mode fault
     * BR[2:0] (bits 5:3): Baud rate = fPCLK / 16 = 16 MHz / 16 = 1 MHz
     *   BR = 011 → (0x3 << 3)
     * SPE  (bit 6):   SPI peripheral enable
     *
     * Bits left at default (0):
     *   CPOL=0, CPHA=0 → SPI Mode 0 (clock idles LOW, sample on rising edge)
     *   DFF=0          → 8-bit data frame
     *   LSBFIRST=0     → MSB transmitted first */
    SPI2->CR1 = (1 << 2)             /* MSTR: master mode */
              | (1 << 9)             /* SSM: software slave management */
              | (1 << 8)             /* SSI: internal slave select HIGH */
              | (0x3 << 3)           /* BR = 011: fPCLK / 16 = 1 MHz */
              | (1 << 6);            /* SPE: enable SPI2 */
}







/*
 * Sends one byte and simultaneously receives one byte over SPI2.
 * SPI is full-duplex: every byte sent on MOSI produces a byte on MISO.
 * When reading from a sensor, pass a dummy byte (0x00) and use the return value.
 * When writing to a sensor, pass the real data and ignore the return value.
 */
uint8_t spi2_transmit_receive(uint8_t byte)
{
    /* Wait until TXE (bit 1) in the status register is set.
     * TXE = 1 means the transmit buffer is empty and ready for a new byte.
     * TXE = 0 means the previous byte hasn't been moved to the shift
     * register yet, so we must wait. */
    while (!(SPI2->SR & (1 << 1)))
    {
        /* Do nothing — just keep checking */
    }

    /* Write the byte to the data register.
     * This loads it into the transmit buffer, and the hardware begins
     * clocking it out on MOSI. Simultaneously, a byte from the slave
     * is clocked in on MISO. */
    SPI2->DR = byte;

    /* Wait until RXNE (bit 0) in the status register is set.
     * RXNE = 1 means the receive buffer has a byte ready to read.
     * RXNE = 0 means the transfer is still in progress. */
    while (!(SPI2->SR & (1 << 0)))
    {
        /* Do nothing — just keep checking */
    }

    /* Read the data register to get the byte received from the slave.
     * Reading DR also clears the RXNE flag automatically. */
    return (uint8_t)SPI2->DR;
}





/*
 * Pulls CS (PB12) LOW to select the BME280.
 * Call this before starting an SPI transaction.
 */
void spi2_cs_enable(void)
{
    GPIOB->ODR &= ~(1 << 12);
}




/*
 * Pulls CS (PB12) HIGH to deselect the BME280.
 * Call this after an SPI transaction is complete.
 */
void spi2_cs_disable(void)
{
    GPIOB->ODR |= (1 << 12);
}


