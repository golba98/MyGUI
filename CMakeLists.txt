cmake_minimum_required(VERSION 3.20)
project(MyGUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

function(mygui_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(MyGUI_core STATIC
    src/Window.cpp src/Event.cpp src/Input.cpp src/Renderer.cpp
    src/Widget.cpp src/Panel.cpp src/Container.cpp src/UIContext.cpp
    src/Font.cpp src/Label.cpp src/Button.cpp src/gl/Functions.cpp
)
target_include_directories(MyGUI_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(MyGUI_core PUBLIC glfw PRIVATE OpenGL::GL Freetype::Freetype)
mygui_warnings(MyGUI_core)

add_executable(MyGUI src/main.cpp)
target_link_libraries(MyGUI PRIVATE MyGUI_core)
mygui_warnings(MyGUI)
add_custom_command(TARGET MyGUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:MyGUI>/assets
    COMMENT "Copying bundled font assets"
)

include(CTest)
if(BUILD_TESTING)
    function(mygui_test target source name)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE MyGUI_core)
        target_compile_definitions(${target} PRIVATE
            MYGUI_TEST_FONT="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSans.ttf")
        mygui_warnings(${target})
        add_test(NAME ${name} COMMAND ${target})
    endfunction()
    mygui_test(MyGUI_tests tests/RendererTests.cpp RendererTests)
    mygui_test(MyGUI_window_tests tests/WindowEventTests.cpp WindowEventTests)
    mygui_test(MyGUI_ui_tests tests/UIContextTests.cpp UIContextTests)
    mygui_test(MyGUI_font_tests tests/FontTests.cpp FontTests)

    # This target has no GLFW, FreeType, OpenGL, or display dependency.
    add_executable(MyGUI_input_tests tests/InputEventTests.cpp src/Event.cpp src/Input.cpp)
    target_include_directories(MyGUI_input_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    mygui_warnings(MyGUI_input_tests)
    add_test(NAME InputEventTests COMMAND MyGUI_input_tests)
    set_tests_properties(RendererTests WindowEventTests PROPERTIES LABELS "display")
    set_tests_properties(InputEventTests UIContextTests FontTests PROPERTIES LABELS "headless")
endif()
