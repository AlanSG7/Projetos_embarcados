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
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Tarefa_U5_C3_Datalogger C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Para incluir os arquivos necessários 
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

# Add executable. Default name is the project name, version 0.1

add_executable(Tarefa_U5_C3_Datalogger Tarefa_U5_C3_Datalogger.c hw_config.c ssd1306.c)

pico_set_program_name(Tarefa_U5_C3_Datalogger "Tarefa_U5_C3_Datalogger")
pico_set_program_version(Tarefa_U5_C3_Datalogger "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Tarefa_U5_C3_Datalogger 0)
pico_enable_stdio_usb(Tarefa_U5_C3_Datalogger 1)

# Add the standard library to the build
target_link_libraries(Tarefa_U5_C3_Datalogger
        pico_stdlib
         hardware_i2c 
         no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)

# Add the standard include files to the build
target_include_directories(Tarefa_U5_C3_Datalogger PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(Tarefa_U5_C3_Datalogger 
        
        )

pico_add_extra_outputs(Tarefa_U5_C3_Datalogger)

# EXIBE USO DE MEMÓRIA APÓS A COMPILAÇÃO
add_custom_command(TARGET Tarefa_U5_C3_Datalogger POST_BUILD COMMAND arm-none-eabi-size 
${CMAKE_CURRENT_BINARY_DIR}/Tarefa_U5_C3_Datalogger.elf )
