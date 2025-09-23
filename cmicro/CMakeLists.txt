cmake_minimum_required(VERSION 3.20)
project(cmicro VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(cmicro
    src/main.c
    src/lexer.c
    src/error.c
    src/parser.c
    src/codegen.c
)

target_include_directories(cmicro PRIVATE include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -g -fno-rtti -fno-exceptions")
option(USE_SANITIZERS "Enable sanitizers" ON)
if (USE_SANITIZERS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=address")
    target_link_options(cmicro PRIVATE -fsanitize=undefined -fsanitize=address)
endif()