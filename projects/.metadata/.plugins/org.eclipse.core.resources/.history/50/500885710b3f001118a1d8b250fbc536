#include "stm32f407xx.h"
#include "usart2.h"

int main(void)
{
    /* Initialize USART2: clocks, GPIO, baud rate, enable */
    usart2_init();

    /* Send a test message to confirm serial output works */
    usart2_send_string("Air Quality Monitor - USART2 OK\r\n");

    /* Loop forever */
    while (1)
    {

    }
}
