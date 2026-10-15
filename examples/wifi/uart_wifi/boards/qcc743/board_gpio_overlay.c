#include "board_gpio.h"
#include "qcc74x_gpio.h"

void board_uartx_gpio_init(void)
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_14, GPIO_UART_FUNC_UART1_TX);
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_15, GPIO_UART_FUNC_UART1_RX);
}
