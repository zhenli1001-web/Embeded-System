set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Some default GCC settings
set(TOOLCHAIN_PREFIX                arm-none-eabi-)

set(_stm32cube_gnu_tools_root "$ENV{HOME}/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32")
file(GLOB _stm32cube_toolchain_bins LIST_DIRECTORIES true "${_stm32cube_gnu_tools_root}/*/bin")
find_program(_resolved_arm_gcc
    NAMES ${TOOLCHAIN_PREFIX}gcc
    PATHS ${_stm32cube_toolchain_bins}
    NO_DEFAULT_PATH
)

if(NOT _resolved_arm_gcc)
    find_program(_resolved_arm_gcc NAMES ${TOOLCHAIN_PREFIX}gcc)
endif()

if(_resolved_arm_gcc)
    get_filename_component(_toolchain_bin_dir "${_resolved_arm_gcc}" DIRECTORY)
    set(CMAKE_C_COMPILER            "${_resolved_arm_gcc}")
    set(CMAKE_ASM_COMPILER          "${_resolved_arm_gcc}")
    set(CMAKE_CXX_COMPILER          "${_toolchain_bin_dir}/${TOOLCHAIN_PREFIX}g++")
    set(CMAKE_LINKER                "${_toolchain_bin_dir}/${TOOLCHAIN_PREFIX}g++")
    set(CMAKE_OBJCOPY               "${_toolchain_bin_dir}/${TOOLCHAIN_PREFIX}objcopy")
    set(CMAKE_SIZE                  "${_toolchain_bin_dir}/${TOOLCHAIN_PREFIX}size")
else()
    set(CMAKE_C_COMPILER            ${TOOLCHAIN_PREFIX}gcc)
    set(CMAKE_ASM_COMPILER          ${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER          ${TOOLCHAIN_PREFIX}g++)
    set(CMAKE_LINKER                ${TOOLCHAIN_PREFIX}g++)
    set(CMAKE_OBJCOPY               ${TOOLCHAIN_PREFIX}objcopy)
    set(CMAKE_SIZE                  ${TOOLCHAIN_PREFIX}size)
endif()

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32L476XX_FLASH.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
