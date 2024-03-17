# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PROJECT "amiga_keyboard")

# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(${PROJECT} C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(${PROJECT} src/main.c )

pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/src/keyboard.pio)
pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/src/amiga.pio)

pico_set_program_name(${PROJECT} ${PROJECT})
pico_set_program_version(${PROJECT} "0.1")

# enable logging over uart, diable over usb
pico_enable_stdio_uart(${PROJECT} 1)
pico_enable_stdio_usb(${PROJECT} 0)

target_include_directories(${PROJECT} BEFORE PUBLIC ${CMAKE_CURRENT_LIST_DIR} include)

# Add the standard library, tiny usb, pio etc to the build
target_link_libraries(${PROJECT} PUBLIC pico_stdlib hardware_pio tinyusb_host tinyusb_board)

pico_add_extra_outputs(${PROJECT})

add_custom_command(TARGET ${PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy amiga_keyboard.uf2 /mnt/c/Users/Neil/Downloads
)
