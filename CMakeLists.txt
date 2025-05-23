cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "D:/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(LabControl C CXX ASM)
pico_sdk_init()

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(LabControl LabControl.c lib/ssd1306.c)

target_include_directories(LabControl PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_set_program_name(LabControl "LabControl")
pico_set_program_version(LabControl "0.1")

pico_enable_stdio_uart(LabControl 0)
pico_enable_stdio_usb(LabControl 0)

target_link_libraries(LabControl
        pico_stdlib
        hardware_gpio
        hardware_pwm
        hardware_i2c
        hardware_adc
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4)

pico_add_extra_outputs(LabControl)

