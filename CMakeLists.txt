cmake_minimum_required(VERSION 3.20)

project(GUIFramework
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED CONFIG)

add_executable(gui_demo
    src/main.cpp
)

target_link_libraries(gui_demo PRIVATE SDL3::SDL3)
