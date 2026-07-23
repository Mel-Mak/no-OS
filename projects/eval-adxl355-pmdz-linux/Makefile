NO_OS = ../..

CC = gcc
CFLAGS = -g -Wall -DLINUX_PLATFORM

INC_DIRS = \
	$(NO_OS)/include \
	$(NO_OS)/drivers/accel/adxl355 \
	$(NO_OS)/drivers/platform/linux \
	$(NO_OS)/drivers/api \
	src

CFLAGS += $(addprefix -I,$(INC_DIRS))

SRCS = \
	src/main.c \
	$(NO_OS)/drivers/accel/adxl355/adxl355.c \
	$(NO_OS)/drivers/api/no_os_spi.c \
	$(NO_OS)/drivers/api/no_os_i2c.c \
	$(NO_OS)/drivers/api/no_os_uart.c \
	$(NO_OS)/drivers/platform/linux/linux_spi.c \
	$(NO_OS)/drivers/platform/linux/linux_i2c.c \
	$(NO_OS)/drivers/platform/linux/linux_uart.c \
	$(NO_OS)/drivers/platform/linux/linux_delay.c \
	$(NO_OS)/util/no_os_util.c \
	$(NO_OS)/util/no_os_alloc.c \
	$(NO_OS)/util/no_os_mutex.c

TARGET = adxl355

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm -lpthread

clean:
	rm -f $(TARGET)
