#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#include "linux_gpio.h"
#include "linux_i2c.h"
#include "linux_spi.h"

#define SPI_OPS                         (&linux_spi_ops)
#define GPIO_OPS                        (&linux_gpio_ops)
#define I2C_OPS                         (&linux_i2c_ops)

/* ADAR1000 beamformers: /dev/spidev0.0 and /dev/spidev0.1. */
#define ADAR1000_SPI_DEVICE             0U
#define ADAR1000_SPI_CS_0               0U
#define ADAR1000_SPI_CS_1               1U
#define ADAR1000_SPI_SPEED              3600000U
#define ADAR1000_DEV_ADDR               0U

/* ADF4159: /dev/spidev0.2, enabled by the three-CS SPI overlay. */
#define ADF4159_SPI_DEVICE              0U
#define ADF4159_SPI_CS                  2U
#define ADF4159_SPI_SPEED               3600000U
#define ADF4159_GPIO_LE                 (-1)

/* AD7291 monitor. */
#define AD7291_I2C_BUS                  1U
#define AD7291_I2C_ADDR                 0x2AU

/* CN0566 GPIO assignments. */
#define GPIO_DIV_S0                     4U
#define GPIO_DIV_S1                     5U
#define GPIO_DIV_S2                     6U
#define GPIO_TX_SW                      12U
#define GPIO_DIV_MR                     13U
#define GPIO_VCTRL_1                    18U
#define GPIO_TR                         22U
#define GPIO_BURST                      23U
#define GPIO_VCTRL_2                    24U
#define GPIO_MUXOUT                     25U
#define GPIO_RX_LOAD                    17U

/* Expected receive-mode GPIO levels used by the reference CN0566 setup. */
#define GPIO_LEVEL_BURST_RX             1U
#define GPIO_LEVEL_VCTRL_1_RX           1U
#define GPIO_LEVEL_VCTRL_2_RX           1U
#define GPIO_LEVEL_DIV_MR_RX            0U
#define GPIO_LEVEL_DIV_S0_RX            0U
#define GPIO_LEVEL_DIV_S1_RX            0U
#define GPIO_LEVEL_DIV_S2_RX            0U
#define GPIO_LEVEL_RX_LOAD_IDLE         0U
#define GPIO_LEVEL_TR_RX                0U
#define GPIO_LEVEL_TX_SW_RX             0U

#endif /* PARAMETERS_H_ */
