# Toolchain Arm GNU (arm-none-eabi-gcc) para os dois alvos STM32.
#
# Procura o compilador, nesta ordem: -DARM_TOOLCHAIN_DIR=..., variavel de
# ambiente ARM_TOOLCHAIN_DIR, PATH, e %USERPROFILE%/tools/arm-gnu-toolchain/bin
# (onde o README manda extrair o zip da Arm).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT ARM_TOOLCHAIN_DIR AND DEFINED ENV{ARM_TOOLCHAIN_DIR})
  set(ARM_TOOLCHAIN_DIR "$ENV{ARM_TOOLCHAIN_DIR}")
endif()
if(NOT ARM_TOOLCHAIN_DIR)
  find_program(_ARM_GCC arm-none-eabi-gcc
    PATHS "$ENV{USERPROFILE}/tools/arm-gnu-toolchain/bin" "$ENV{HOME}/tools/arm-gnu-toolchain/bin")
  if(NOT _ARM_GCC)
    message(FATAL_ERROR "arm-none-eabi-gcc nao encontrado: passe -DARM_TOOLCHAIN_DIR=<pasta bin da Arm GNU Toolchain>")
  endif()
  get_filename_component(ARM_TOOLCHAIN_DIR "${_ARM_GCC}" DIRECTORY)
endif()
set(ARM_TOOLCHAIN_DIR "${ARM_TOOLCHAIN_DIR}" CACHE PATH "pasta bin da Arm GNU Toolchain")

if(CMAKE_HOST_WIN32)
  set(_EXE ".exe")
endif()
set(CMAKE_C_COMPILER   "${ARM_TOOLCHAIN_DIR}/arm-none-eabi-gcc${_EXE}")
set(CMAKE_ASM_COMPILER "${ARM_TOOLCHAIN_DIR}/arm-none-eabi-gcc${_EXE}")
set(CMAKE_OBJCOPY      "${ARM_TOOLCHAIN_DIR}/arm-none-eabi-objcopy${_EXE}" CACHE FILEPATH "")
set(CMAKE_SIZE         "${ARM_TOOLCHAIN_DIR}/arm-none-eabi-size${_EXE}" CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
