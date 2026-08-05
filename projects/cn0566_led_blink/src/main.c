#include "parameters.h"
#include "no_os_gpio.h"
#include "no_os_delay.h"
#include "no_os_error.h"

#include <stdio.h>

int main(void)
{
	struct no_os_gpio_desc *led;
	int ret;

	struct no_os_gpio_init_param led_param = {
		.port = GPIO_LED_PORT,
		.number = GPIO_LED_PIN,
		.platform_ops = GPIO_OPS,
		.extra = NULL,
	};

	ret = no_os_gpio_get(&led, &led_param);
	if (ret) {
		printf("GPIO get failed: %d\n", ret);
		return ret;
	}

	ret = no_os_gpio_direction_output(led, NO_OS_GPIO_LOW);
	if (ret) {
		printf("GPIO direction failed: %d\n", ret);
		goto cleanup;
	}

	printf("Blinking LED on GPIO %d...\n", GPIO_LED_PIN);

	while (1) {
		no_os_gpio_set_value(led, NO_OS_GPIO_HIGH);
		no_os_mdelay(500);
		no_os_gpio_set_value(led, NO_OS_GPIO_LOW);
		no_os_mdelay(500);
	}

cleanup:
	no_os_gpio_remove(led);

	return ret;
}
