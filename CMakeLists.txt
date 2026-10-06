cmake_minimum_required(VERSION 3.20)

project(MyGUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_library(MyGUI_core STATIC
    src/Window.cpp
    src/Event.cpp
    src/Input.cpp
    src/Renderer.cpp
    src/Widget.cpp
    src/Panel.cpp
    src/gl/Functions.cpp
)

target_include_directories(MyGUI_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MyGUI_core
    PUBLIC glfw
    PRIVATE OpenGL::GL
)

add_executable(MyGUI
    src/main.cpp
)

target_link_libraries(MyGUI PRIVATE MyGUI_core)

enable_testing()

add_executable(MyGUI_tests
    tests/RendererTests.cpp
)

target_link_libraries(MyGUI_tests PRIVATE MyGUI_core)

add_test(NAME RendererTests COMMAND MyGUI_tests)

# Pure event/state tests have no GLFW, OpenGL, or display dependency.
add_executable(MyGUI_input_tests
    tests/InputEventTests.cpp
    src/Event.cpp
    src/Input.cpp
)
target_include_directories(MyGUI_input_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME InputEventTests COMMAND MyGUI_input_tests)

add_executable(MyGUI_window_tests tests/WindowEventTests.cpp)
target_link_libraries(MyGUI_window_tests PRIVATE MyGUI_core)
add_test(NAME WindowEventTests COMMAND MyGUI_window_tests)

foreach(target MyGUI_core MyGUI MyGUI_tests MyGUI_input_tests MyGUI_window_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
