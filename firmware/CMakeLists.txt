set(TP ${PROJECT_SOURCE_DIR}/third_party)

set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

# The kernel, built with its own (quieter) warnings
add_library(freertos STATIC
    ${TP}/FreeRTOS-Kernel/tasks.c
    ${TP}/FreeRTOS-Kernel/queue.c
    ${TP}/FreeRTOS-Kernel/list.c
    ${TP}/FreeRTOS-Kernel/timers.c
    ${TP}/FreeRTOS-Kernel/event_groups.c
    ${TP}/FreeRTOS-Kernel/stream_buffer.c
    ${TP}/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c
    ${TP}/FreeRTOS-Kernel/portable/MemMang/heap_4.c
)
target_include_directories(freertos PUBLIC
    platform
    ${TP}/FreeRTOS-Kernel/include
    ${TP}/FreeRTOS-Kernel/portable/GCC/ARM_CM4F
)
target_compile_options(freertos PRIVATE ${CPU_FLAGS} -ffunction-sections -fdata-sections -Wall)

file(GLOB FIRMWARE_SOURCES CONFIGURE_DEPENDS
    platform/*.c drivers/*.c devices/*.c lib/*.c app/*.c)

add_executable(access-terminal.elf ${FIRMWARE_SOURCES})
target_include_directories(access-terminal.elf PRIVATE
    platform drivers devices lib app
    ${TP}/cmsis/Include
    ${TP}/cmsis_device_f4/Include
)
target_compile_definitions(access-terminal.elf PRIVATE
    STM32F446xx
    FIRMWARE_VERSION="${PROJECT_VERSION}"
)
target_compile_options(access-terminal.elf PRIVATE
    ${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common
    -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter
)
target_link_libraries(access-terminal.elf PRIVATE freertos)
target_link_options(access-terminal.elf PRIVATE
    ${CPU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f446re.ld
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_BINARY_DIR}/access-terminal.map
)
# build/firmware/access-terminal.elf, where sim/project.json loads it
set_target_properties(access-terminal.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f446re.ld)

add_custom_command(TARGET access-terminal.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary access-terminal.elf access-terminal.bin
    COMMAND ${CMAKE_SIZE} access-terminal.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM)
