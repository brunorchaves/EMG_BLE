cmake_minimum_required(VERSION 3.20)

set(BOARD "h563zi" CACHE STRING "Placa alvo: h563zi | f767zi")
set_property(CACHE BOARD PROPERTY STRINGS h563zi f767zi)

project(stm32_dac_player C ASM)

if(BOARD STREQUAL "h563zi")
  set(CPU_FLAGS -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
  set(DEVICE_DEF STM32H563xx)
  set(DEVICE_INC vendor/cmsis_device_h5/Include)
  set(STARTUP boards/h563zi/startup_stm32h563xx.s)
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/boards/h563zi/STM32H563xx_FLASH.ld)
elseif(BOARD STREQUAL "f767zi")
  set(CPU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)
  set(DEVICE_DEF STM32F767xx)
  set(DEVICE_INC vendor/cmsis_device_f7/Include)
  set(STARTUP boards/f767zi/startup_stm32f767xx.s)
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/boards/f767zi/STM32F767xx_FLASH.ld)
else()
  message(FATAL_ERROR "BOARD desconhecida: ${BOARD} (use h563zi ou f767zi)")
endif()

set(TARGET stm32_dac_player_${BOARD})

add_executable(${TARGET}
  common/main.c
  common/player.c
  common/cli.c
  common/stimulus_table.c
  boards/${BOARD}/board.c
  boards/syscalls.c
  ${STARTUP}
)
set_target_properties(${TARGET} PROPERTIES SUFFIX ".elf")

target_include_directories(${TARGET} PRIVATE
  common
  vendor/cmsis_core/Include
  ${DEVICE_INC}
)
target_compile_definitions(${TARGET} PRIVATE ${DEVICE_DEF})
target_compile_options(${TARGET} PRIVATE
  ${CPU_FLAGS}
  $<$<COMPILE_LANGUAGE:C>:-std=c11 -O2 -g3 -Wall -Wextra -Wshadow -Wdouble-promotion -ffunction-sections -fdata-sections>
  $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
)
target_link_options(${TARGET} PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  --specs=nano.specs --specs=nosys.specs
  -Wl,--gc-sections -Wl,--print-memory-usage
  -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET}.map
)
target_link_libraries(${TARGET} PRIVATE m)
set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS ${LINKER_SCRIPT})

add_custom_command(TARGET ${TARGET} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${CMAKE_BINARY_DIR}/${TARGET}.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> ${CMAKE_BINARY_DIR}/${TARGET}.hex
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
  COMMENT "Gerando ${TARGET}.bin / .hex"
)
