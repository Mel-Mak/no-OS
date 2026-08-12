#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#include "linux_i2c.h"
#include "linux_spi.h"
#include "linux_gpio.h"

/* AD7291 I2C parameters */
#define AD7291_I2C_BUS		1
#define AD7291_I2C_ADDR		0x2A
#define I2C_OPS			&linux_i2c_ops

/* ADF4159 SPI parameters */
#define ADF4159_SPI_DEVICE	0
#define ADF4159_SPI_CS		0
#define ADF4159_SPI_SPEED	3600000
#define SPI_OPS			&linux_spi_ops

/* ADF4159 GPIO LE (latch enable) GPIO 27 on CN0566 */
#define ADF4159_GPIO_LE		27
#define GPIO_OPS		&linux_gpio_ops

#endif /* __PARAMETERS_H__ */
