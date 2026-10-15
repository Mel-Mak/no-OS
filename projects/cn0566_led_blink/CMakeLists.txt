cmake_minimum_required(VERSION 3.10)
project(cn0566_led_blink C)

set(NO_OS ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_compile_options(-Wall -DLINUX_PLATFORM)

add_executable(cn0566_led_blink
    src/main.c
    ${NO_OS}/drivers/api/no_os_gpio.c
    ${NO_OS}/drivers/platform/linux-userspace/linux_gpio.c
    ${NO_OS}/drivers/platform/linux-userspace/linux_delay.c
    ${NO_OS}/util/no_os_alloc.c
    ${NO_OS}/util/no_os_mutex.c
)

target_include_directories(cn0566_led_blink PRIVATE
    src
    ${NO_OS}/include
    ${NO_OS}/drivers/platform/linux-userspace
    ${NO_OS}/drivers/api
)

target_link_libraries(cn0566_led_blink pthread)
