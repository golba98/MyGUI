# Builds against an installed MyGUI to check the exported package:
#   cmake -S tests/package -B build/package -DCMAKE_PREFIX_PATH=<install prefix>
cmake_minimum_required(VERSION 3.20)
project(MyGUIConsumer LANGUAGES CXX)

find_package(MyGUI 0.1 REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE MyGUI::MyGUI)
