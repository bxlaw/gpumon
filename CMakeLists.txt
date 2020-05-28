cmake_minimum_required(VERSION 3.17)

project(gpumon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIRS})

add_executable(gpumon main.cpp)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if (ipo_supported)
        set_property(TARGET gpumon PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

target_compile_options(gpumon PRIVATE -Wall -Wextra -Wpedantic ${CURSES_CFLAGS})
target_link_options(gpumon PRIVATE -Wl,--as-needed)
target_link_libraries(gpumon ${CURSES_LIBRARIES})
