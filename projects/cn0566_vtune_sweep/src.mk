NO_OS_INC_DIRS += \
	$(INCLUDE) \
	$(PROJECT)/src/ \
	$(DRIVERS)/api/

INCS += $(DRIVERS)/power/ad7291/ad7291.h \
	$(DRIVERS)/frequency/adf4159/adf4159.h \
	$(PLATFORM_DRIVERS)/linux_i2c.h \
	$(PLATFORM_DRIVERS)/linux_spi.h \
	$(PLATFORM_DRIVERS)/linux_gpio.h

SRCS += $(PROJECT)/src/main.c \
	$(DRIVERS)/power/ad7291/ad7291.c \
	$(DRIVERS)/frequency/adf4159/adf4159.c \
	$(DRIVERS)/api/no_os_i2c.c \
	$(DRIVERS)/api/no_os_spi.c \
	$(DRIVERS)/api/no_os_gpio.c \
	$(PLATFORM_DRIVERS)/linux_i2c.c \
	$(PLATFORM_DRIVERS)/linux_spi.c \
	$(PLATFORM_DRIVERS)/linux_gpio.c \
	$(PLATFORM_DRIVERS)/linux_delay.c \
	$(NO-OS)/util/no_os_util.c \
	$(NO-OS)/util/no_os_alloc.c \
	$(NO-OS)/util/no_os_mutex.c
