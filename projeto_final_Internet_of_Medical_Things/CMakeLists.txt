# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# Para o TLS
set(PICO_ENABLE_MBEDTLS 1)
# TLS para a pi pico w
set(PICO_MBEDTLS_PATH ${PICO_SDK_PATH}/lib/mbedtls)

# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)


# Pull in FreeRTOS
include(/Users/alans/OneDrive/Documentos/Projetos_pi_pico/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(U7_projeto_final C CXX ASM)


# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(U7_projeto_final U7_projeto_final.c ssd1306.c sensores.c conectividade.c)

pico_set_program_name(U7_projeto_final "U7_projeto_final")
pico_set_program_version(U7_projeto_final "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(U7_projeto_final 0)
pico_enable_stdio_usb(U7_projeto_final 1)

# Add the standard library to the build
target_link_libraries(U7_projeto_final
        pico_stdlib
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4
        hardware_i2c
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_mqtt
        pico_lwip_mbedtls
        pico_mbedtls
        hardware_adc
        hardware_pwm)

# Add the standard include files to the build
target_include_directories(U7_projeto_final PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(U7_projeto_final 
        
        )

pico_add_extra_outputs(U7_projeto_final)

# EXIBE USO DE MEMÓRIA APÓS A COMPILAÇÃO
add_custom_command(TARGET U7_projeto_final POST_BUILD COMMAND arm-none-eabi-size ${CMAKE_CURRENT_BINARY_DIR}/U7_projeto_final.elf )

