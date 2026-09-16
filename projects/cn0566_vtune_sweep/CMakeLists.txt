cmake_minimum_required(VERSION 3.10)
project(cn0566_vtune_sweep C)

set(NO_OS ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_compile_options(-Wall -DLINUX_PLATFORM)

add_executable(cn0566_vtune_sweep
    src/main.c
    ${NO_OS}/drivers/adc/ad7291/ad7291.c
    ${NO_OS}/drivers/frequency/adf4159/adf4159.c
    ${NO_OS}/drivers/api/no_os_i2c.c
    ${NO_OS}/drivers/api/no_os_spi.c
    ${NO_OS}/drivers/api/no_os_gpio.c
    ${NO_OS}/drivers/platform/linux/linux_i2c.c
    ${NO_OS}/drivers/platform/linux/linux_spi.c
    ${NO_OS}/drivers/platform/linux/linux_gpio.c
    ${NO_OS}/drivers/platform/linux/linux_delay.c
    ${NO_OS}/util/no_os_util.c
    ${NO_OS}/util/no_os_alloc.c
    ${NO_OS}/util/no_os_mutex.c
)

target_include_directories(cn0566_vtune_sweep PRIVATE
    src
    ${NO_OS}/include
    ${NO_OS}/drivers/adc/ad7291
    ${NO_OS}/drivers/frequency/adf4159
    ${NO_OS}/drivers/platform/linux
    ${NO_OS}/drivers/api
)

target_link_libraries(cn0566_vtune_sweep m pthread)
