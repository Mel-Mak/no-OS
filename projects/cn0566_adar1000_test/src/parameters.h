#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#include "linux_spi.h"
#include "linux_gpio.h"

/* ADF4159 SPI parameters � spidev0.2 uses CS2 = GPIO27 (hardware-managed LE).
 * Requires spi0-3cs-spidev overlay loaded in /boot/config.txt. */
#define ADF4159_SPI_DEVICE	0
#define ADF4159_SPI_CS		2
#define ADF4159_SPI_SPEED	3600000
#define SPI_OPS			&linux_spi_ops

/* LE managed by SPI controller (CS2 = GPIO27), no manual toggle needed */
#define ADF4159_GPIO_LE		-1
#define GPIO_OPS		&linux_gpio_ops

/* ADAR1000 beamformers � spidev0.0 (CS0) and spidev0.1 (CS1).
 * Each CS line has one ADAR1000 at device address 0. */
#define ADAR1000_SPI_DEVICE	0
#define ADAR1000_SPI_CS_0	0
#define ADAR1000_SPI_CS_1	1
#define ADAR1000_SPI_SPEED	3600000
#define ADAR1000_DEV_ADDR	0

/* CN0566 LO routing switches (ADRF5019, sheet 9) */
#define GPIO_VCTRL_1		18
#define GPIO_VCTRL_2		24

/* CN0566 board GPIOs (one-bit-adc-dac in rpi-cn0566 overlay) */
#define GPIO_DIV_S0		4
#define GPIO_DIV_S1		5
#define GPIO_DIV_S2		6
#define GPIO_DIV_MR		13
#define GPIO_RX_LOAD		17
#define GPIO_TR			22
#define GPIO_TX_SW		12
#define GPIO_MUXOUT		25
#define GPIO_BURST		23

#endif /* __PARAMETERS_H__ */
