cmake_minimum_required(VERSION 3.20)
project(MyGUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# OpenGL entry points are loaded at runtime through GLFW, so the library
# needs no OpenGL link dependency.
find_package(glfw3 REQUIRED)
find_package(Freetype REQUIRED)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(MYGUI_TOP_LEVEL ON)
else()
    set(MYGUI_TOP_LEVEL OFF)
endif()
option(MYGUI_INSTALL "Generate install rules and a CMake package" ${MYGUI_TOP_LEVEL})

function(mygui_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(WIN32)
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
endfunction()

add_library(MyGUI_core STATIC
    src/Window.cpp src/Event.cpp src/Input.cpp src/Renderer.cpp
    src/Widget.cpp src/Panel.cpp src/Container.cpp src/UIContext.cpp
    src/Font.cpp src/Label.cpp src/Button.cpp src/Image.cpp src/ImageView.cpp
    src/Checkbox.cpp src/Slider.cpp src/ProgressBar.cpp src/ScrollView.cpp
    src/Popup.cpp src/ComboBox.cpp src/Dialog.cpp src/TextField.cpp
    src/gl/Functions.cpp
)
add_library(MyGUI::MyGUI ALIAS MyGUI_core)
set_target_properties(MyGUI_core PROPERTIES EXPORT_NAME MyGUI)
target_compile_features(MyGUI_core PUBLIC cxx_std_20)
target_include_directories(MyGUI_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# Vendored stb_image and Khronos headers: private, and SYSTEM so their
# warnings stay out of ours.
target_include_directories(MyGUI_core SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/khronos)
# Public headers only forward-declare GLFW types.
target_link_libraries(MyGUI_core PRIVATE glfw Freetype::Freetype)
mygui_warnings(MyGUI_core)

option(MYGUI_BUILD_EXAMPLES "Build the demo and examples" ${MYGUI_TOP_LEVEL})
if(MYGUI_BUILD_EXAMPLES)
    # Every example shares one output directory and the assets copied into it.
    add_custom_target(MyGUI_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:MyGUI>/assets
        COMMENT "Copying bundled font assets"
    )
    function(mygui_example target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE MyGUI::MyGUI)
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/common)
        mygui_warnings(${target})
        add_dependencies(${target} MyGUI_assets)
    endfunction()
    mygui_example(MyGUI examples/demo/main.cpp)
    mygui_example(MyGUI_hello examples/hello.cpp)
    mygui_example(MyGUI_layout examples/layout.cpp)
    mygui_example(MyGUI_widgets examples/widgets.cpp)
    mygui_example(MyGUI_dialogs examples/dialogs.cpp)
    mygui_example(MyGUI_combobox examples/combobox.cpp)
endif()

if(MYGUI_INSTALL)
    install(TARGETS MyGUI_core EXPORT MyGUITargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY include/gui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES assets/fonts/NotoSans.ttf assets/fonts/OFL.txt assets/fonts/SOURCE.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/MyGUI/fonts)
    set(MYGUI_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/MyGUI)
    install(EXPORT MyGUITargets NAMESPACE MyGUI:: DESTINATION ${MYGUI_CMAKE_DIR})
    configure_package_config_file(cmake/MyGUIConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/MyGUIConfig.cmake INSTALL_DESTINATION ${MYGUI_CMAKE_DIR})
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/MyGUIConfigVersion.cmake
        COMPATIBILITY SameMinorVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/MyGUIConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/MyGUIConfigVersion.cmake
        DESTINATION ${MYGUI_CMAKE_DIR})
endif()

# Tests belong to standalone builds; a parent project's testing settings are left alone.
if(MYGUI_TOP_LEVEL)
    include(CTest)
endif()
if(MYGUI_TOP_LEVEL AND BUILD_TESTING)
    function(mygui_test target source name)
        add_executable(${target} ${source})
        # Display tests drive GLFW directly to create contexts and inject input.
        target_link_libraries(${target} PRIVATE MyGUI::MyGUI glfw)
        target_include_directories(${target} SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/khronos)
        target_compile_definitions(${target} PRIVATE
            MYGUI_TEST_FONT="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSans.ttf")
        mygui_warnings(${target})
        add_test(NAME ${name} COMMAND ${target})
    endfunction()
    mygui_test(MyGUI_tests tests/RendererTests.cpp RendererTests)
    mygui_test(MyGUI_window_tests tests/WindowEventTests.cpp WindowEventTests)
    mygui_test(MyGUI_popup_tests tests/PopupTests.cpp PopupTests)
    mygui_test(MyGUI_combobox_tests tests/ComboBoxTests.cpp ComboBoxTests)
    mygui_test(MyGUI_dialog_tests tests/DialogTests.cpp DialogTests)
    mygui_test(MyGUI_ui_tests tests/UIContextTests.cpp UIContextTests)
    mygui_test(MyGUI_font_tests tests/FontTests.cpp FontTests)
    mygui_test(MyGUI_image_tests tests/ImageTests.cpp ImageTests)

    # This target has no GLFW, FreeType, OpenGL, or display dependency.
    add_executable(MyGUI_input_tests tests/InputEventTests.cpp src/Event.cpp src/Input.cpp)
    target_include_directories(MyGUI_input_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    mygui_warnings(MyGUI_input_tests)
    add_test(NAME InputEventTests COMMAND MyGUI_input_tests)
    set_tests_properties(RendererTests WindowEventTests PROPERTIES LABELS "display")
    set_tests_properties(PopupTests ComboBoxTests DialogTests InputEventTests UIContextTests FontTests ImageTests PROPERTIES LABELS "headless")
endif()
