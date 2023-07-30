cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(gc)

add_library(gc_lib INTERFACE)
target_include_directories(gc_lib
INTERFACE
	include
)

add_executable(gc_test
	src/main.cpp
)

target_link_libraries(gc_test
PRIVATE
	gc_lib
)
