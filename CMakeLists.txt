cmake_minimum_required(VERSION 3.20)

project(MyGUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(MyGUI
    src/main.cpp
    src/Window.cpp
)

target_include_directories(MyGUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MyGUI PRIVATE
    glfw
    OpenGL::GL
)
