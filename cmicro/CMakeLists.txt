cmake_minimum_required(VERSION 3.20)
project(cmicro VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    message(STATUS "Found ZLIB: ${ZLIB_LIBRARIES}")
else()
    message(FATAL_ERROR "ZLIB not found. Please install zlib1g-dev on Debian or equivalent.")
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

execute_process(
    COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --libs all
    OUTPUT_VARIABLE LLVM_LIBRARIES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE " " ";" LLVM_LIBRARIES "${LLVM_LIBRARIES}")

add_executable(cmicro
    src/main.c
    src/lexer.c
    src/error.c
    src/parser.c
    src/codegen.c
)

target_include_directories(cmicro PRIVATE
    include
    ${LLVM_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

target_compile_definitions(cmicro PRIVATE
    ${LLVM_DEFINITIONS}
)

target_link_libraries(cmicro PRIVATE
    ${LLVM_LIBRARIES}
    ZLIB::ZLIB
    stdc++
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -g -fno-rtti -fno-exceptions")
option(USE_SANITIZERS "Enable sanitizers" ON)
if (USE_SANITIZERS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=address")
    target_link_options(cmicro PRIVATE -fsanitize=undefined -fsanitize=address)
endif()