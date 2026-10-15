#include "qcc743_glb_gpio.h"

void qcc74x_gpio_pad_check(uint8_t pin)
{
    if (GLB_GPIO_Pad_LeadOut_Sts(pin) == RESET) {
        printf("Error:GPIO%d doesn't exist on this chip,please choose others!!\r\n",pin);
        while (1) {}
    }
}