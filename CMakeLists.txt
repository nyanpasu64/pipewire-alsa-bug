cmake_minimum_required(VERSION 3.14)

project(AudioLibraryRosettaStone VERSION 0.1 LANGUAGES C CXX)

## Configure global settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND options
        -Wall -Wextra -Wconversion -Wsign-conversion
        -Wmissing-declarations
        -Wno-unused-function -Wno-unused-parameter
        -Werror=return-type
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND options
            -Wtautological-unsigned-zero-compare
            -Wmissing-variable-declarations
        )
    endif()
endif ()

## Add dependencies

find_package(ALSA REQUIRED)

## Application

add_executable(alsa alsa.cpp)
target_compile_options(alsa PRIVATE "${options}")
target_link_libraries(alsa PRIVATE ALSA::ALSA)

add_executable(pcm_min pcm_min.c)
target_compile_options(pcm_min PRIVATE "${options}")
target_link_libraries(pcm_min PRIVATE ALSA::ALSA)
