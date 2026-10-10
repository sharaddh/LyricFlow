cmake_minimum_required(VERSION 3.21)
project(LyricFlow VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(LyricFlow
    src/main.cpp
    src/OverlayWindow.h
    src/OverlayWindow.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
    src/LyricsSearchDialog.h
    src/LyricsSearchDialog.cpp
    src/LyricsClient.h
    src/LyricsClient.cpp
)

target_link_libraries(LyricFlow PRIVATE Qt6::Widgets Qt6::Network)

if(WIN32)
    target_compile_definitions(LyricFlow PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
