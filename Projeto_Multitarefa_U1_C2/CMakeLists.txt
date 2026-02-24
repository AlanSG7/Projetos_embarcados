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

# Pull in FreeRTOS
include(/Users/alans/OneDrive/Documentos/Projetos_pi_pico/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(Projeto_Multitarefa_U1_C2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Projeto_Multitarefa_U1_C2 Projeto_Multitarefa_U1_C2.c )

pico_set_program_name(Projeto_Multitarefa_U1_C2 "Projeto_Multitarefa_U1_C2")
pico_set_program_version(Projeto_Multitarefa_U1_C2 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Projeto_Multitarefa_U1_C2 0)
pico_enable_stdio_usb(Projeto_Multitarefa_U1_C2 1)

# Add the standard library to the build
target_link_libraries(Projeto_Multitarefa_U1_C2
        pico_stdlib
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4)

# Add the standard include files to the build
target_include_directories(Projeto_Multitarefa_U1_C2 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(Projeto_Multitarefa_U1_C2 
        
        )

pico_add_extra_outputs(Projeto_Multitarefa_U1_C2)

# EXIBE USO DE MEMÓRIA APÓS A COMPILAÇÃO
add_custom_command(TARGET Projeto_Multitarefa_U1_C2 POST_BUILD COMMAND arm-none-eabi-size ${CMAKE_CURRENT_BINARY_DIR}/Projeto_Multitarefa_U1_C2.elf )

