cmake_minimum_required(VERSION 3.16)
project(JezzballQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

add_executable(flingball
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/GameWidget.h
    src/GameWidget.cpp
)

target_link_libraries(flingball PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia)