cmake_minimum_required(VERSION 3.20)
project(mutils)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_library(mutils INTERFACE)
target_include_directories(mutils INTERFACE include/)

option(MUTILS_BUILD_TESTS "Build tests" ON)
if(MUTILS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
