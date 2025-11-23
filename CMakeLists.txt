cmake_minimum_required(VERSION 3.12)

# 设置为交叉编译
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR RISC-V)

# 设置项目名称和语言
project(CH32xPID936 C ASM)

# 禁用编译器测试
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

# 设置编译器
set(CMAKE_C_COMPILER riscv-wch-elf-gcc)
set(CMAKE_ASM_COMPILER riscv-wch-elf-gcc)
set(CMAKE_OBJCOPY riscv-wch-elf-objcopy)
set(CMAKE_OBJDUMP riscv-wch-elf-objdump)
set(CMAKE_SIZE riscv-wch-elf-size)

# 禁用Windows特定选项
set(CMAKE_EXECUTABLE_SUFFIX .elf)
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")  # 移除 -rdynamic
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")  # 移除 -rdynamic
set(CMAKE_SYSTEM_NAME Generic)  # 表明这是一个裸机系统
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")  # 防止测试时链接执行文件

# 编译选项
set(CMAKE_C_FLAGS "-march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -g")
set(CMAKE_ASM_FLAGS "-march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_SOURCE_DIR}/Ld/Link.ld -nostartfiles -Xlinker --gc-sections -Xlinker --print-memory-usage -Wl,-Map,${CMAKE_BINARY_DIR}/CH32xPID936.map --specs=nano.specs --specs=nosys.specs")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

# 禁用所有默认链接库
set(CMAKE_C_STANDARD_LIBRARIES "")

# 添加源文件
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_SOURCE_DIR}/Core/*.c")
file(GLOB_RECURSE DEBUG_SOURCES "${CMAKE_SOURCE_DIR}/Debug/*.c")
file(GLOB_RECURSE FREERTOS_SOURCES 
    "${CMAKE_SOURCE_DIR}/FreeRTOS/*.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/*.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/MemMang/*.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/Common/*.c"
)
file(GLOB_RECURSE PERIPHERAL_SOURCES "${CMAKE_SOURCE_DIR}/Peripheral/src/*.c")
file(GLOB_RECURSE USER_SOURCES "${CMAKE_SOURCE_DIR}/User/*.c")
file(GLOB_RECURSE STARTUP_SOURCES 
    "${CMAKE_SOURCE_DIR}/Startup/*.S"
    "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/*.S"
)

# 包含头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/Core
    ${CMAKE_SOURCE_DIR}/Debug
    ${CMAKE_SOURCE_DIR}/FreeRTOS/include
    ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V
    ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/chip_specific_extensions/RV32I_PFIC_no_extensions
    ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/Common
    ${CMAKE_SOURCE_DIR}/Peripheral/inc
    ${CMAKE_SOURCE_DIR}/User
)

# 创建可执行文件
add_executable(CH32xPID936
    ${CORE_SOURCES}
    ${DEBUG_SOURCES}
    ${FREERTOS_SOURCES}
    ${PERIPHERAL_SOURCES}
    ${USER_SOURCES}
    ${STARTUP_SOURCES}
)

# 设置输出目录
set_target_properties(CH32xPID936 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 链接数学库
target_link_libraries(CH32xPID936 m)

# 生成hex文件
add_custom_command(TARGET CH32xPID936 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:CH32xPID936> ${CMAKE_BINARY_DIR}/CH32xPID936.hex
    COMMAND ${CMAKE_OBJDUMP} --all-headers --demangle --disassemble -M xw $<TARGET_FILE:CH32xPID936> > ${CMAKE_BINARY_DIR}/CH32xPID936.lst
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:CH32xPID936> > ${CMAKE_BINARY_DIR}/CH32xPID936.siz
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
