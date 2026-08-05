SRCS += $(PROJECT)/src/main.c

SRCS += $(DRIVERS)/api/no_os_gpio.c \
	$(NO-OS)/util/no_os_alloc.c \
	$(NO-OS)/util/no_os_mutex.c \
	$(NO-OS)/util/no_os_util.c

SRCS += $(PLATFORM_DRIVERS)/$(PLATFORM)_gpio.c \
	$(PLATFORM_DRIVERS)/$(PLATFORM)_delay.c

INCS += $(INCLUDE)/no_os_gpio.h \
	$(INCLUDE)/no_os_delay.h \
	$(INCLUDE)/no_os_error.h \
	$(INCLUDE)/no_os_alloc.h \
	$(INCLUDE)/no_os_mutex.h \
	$(INCLUDE)/no_os_util.h

INCS += $(PLATFORM_DRIVERS)/$(PLATFORM)_gpio.h

INCS += $(PROJECT)/src/parameters.h