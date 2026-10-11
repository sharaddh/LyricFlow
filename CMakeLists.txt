cmake_minimum_required(VERSION 3.21)
project(Capson VERSION 1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(Capson WIN32
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
    src/app.rc
    src/app.qrc
)

target_link_libraries(Capson PRIVATE Qt6::Widgets Qt6::Network)

# MediaSession.cpp talks to the WinRT media-session API through hand-written COM
# vtables (raw ABI). GCC miscompiles that ABI code above -O0, which crashed the
# app at startup in optimized builds, so this one file is always built unoptimized.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_source_files_properties(src/MediaSession.cpp PROPERTIES COMPILE_OPTIONS "-O0")
endif()

if(WIN32)
    target_compile_definitions(Capson PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    # EntryPointPrivate provides the WinMain that calls our main(), required now
    # that the target is a GUI (WIN32) subsystem app instead of a console app.
    target_link_libraries(Capson PRIVATE user32 Qt6::EntryPointPrivate)
endif()
