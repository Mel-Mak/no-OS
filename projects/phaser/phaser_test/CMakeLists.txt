cmake_minimum_required(VERSION 3.10)
project(phaser_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Point NO_OS_ROOT at the checked-out no-OS repository when this project is
# not located at no-OS/projects/phaser_test.
set(NO_OS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH "no-OS repository root")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBIIO REQUIRED IMPORTED_TARGET libiio)

set(NO_OS_SOURCES
    ${NO_OS_ROOT}/drivers/rf-transceiver/adar1000/adar1000.c
    ${NO_OS_ROOT}/drivers/frequency/adf4159/adf4159.c
    ${NO_OS_ROOT}/drivers/api/no_os_gpio.c
    ${NO_OS_ROOT}/drivers/api/no_os_spi.c
    ${NO_OS_ROOT}/drivers/platform/linux-userspace/linux_gpio.c
    ${NO_OS_ROOT}/drivers/platform/linux-userspace/linux_spi.c
    ${NO_OS_ROOT}/drivers/platform/linux-userspace/linux_delay.c
    ${NO_OS_ROOT}/util/no_os_alloc.c
    ${NO_OS_ROOT}/util/no_os_mutex.c
)

add_executable(phaser_test
    main.c
    iio_capture.c
    ${NO_OS_SOURCES}
)

target_include_directories(phaser_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NO_OS_ROOT}/include
    ${NO_OS_ROOT}/drivers/rf-transceiver/adar1000
    ${NO_OS_ROOT}/drivers/frequency/adf4159
    ${NO_OS_ROOT}/drivers/platform/linux-userspace
)

target_link_libraries(phaser_test PRIVATE PkgConfig::LIBIIO)
target_compile_options(phaser_test PRIVATE -Wall -Wextra -Wpedantic)
