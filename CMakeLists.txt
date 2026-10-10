cmake_minimum_required(VERSION 3.21)
project(Capson VERSION 1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(Capson
    src/main.cpp
    src/OverlayWindow.h
    src/OverlayWindow.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
    src/LyricsSearchDialog.h
    src/LyricsSearchDialog.cpp
    src/LyricsClient.h
    src/LyricsClient.cpp
    src/MediaDetector.h
    src/MediaDetector.cpp
    src/MediaSession.h
    src/MediaSession.cpp
)

target_link_libraries(Capson PRIVATE Qt6::Widgets Qt6::Network)

if(WIN32)
    target_compile_definitions(Capson PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(Capson PRIVATE user32)
endif()
