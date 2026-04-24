#include "stm32f407xx.h"
#include "usart2.h"
#include "spi2.h"
#include "bme280.h"

int main(void)
{
    /* Step 1: Initialize the debug serial port (USART2) first.
     * We need this up and running before anything else so we can print
     * status messages about the rest of the startup sequence. */
    usart2_init();
    usart2_send_string("\r\n--- Air Quality Monitor starting ---\r\n");

    /* Step 2: Initialize the SPI2 peripheral so we can talk to the BME280.
     * This configures GPIO pins PB12-PB15, enables clocks, and sets up
     * SPI Mode 0 at 1 MHz. */
    usart2_send_string("Initializing SPI2... ");
    spi2_init();
    usart2_send_string("OK\r\n");

    /* Step 3: Run the BME280 chip ID check.
     * This sends a read command for register 0xD0 and verifies the
     * response is 0x60. If it fails, the sensor is either not wired
     * correctly, not powered, or not responding on SPI. */
    usart2_send_string("Checking BME280 chip ID... ");
    if (bme280_init())
    {
        usart2_send_string("PASS (0x60 received)\r\n");
    }
    else
    {
        usart2_send_string("FAIL (wrong chip ID)\r\n");
    }

    /* Step 4: Infinite loop. In bare-metal firmware there's no OS to
     * return to, so main() must never exit. */
    while (1)
    {

    }
}
