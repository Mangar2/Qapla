cmake_minimum_required(VERSION 3.18)
project(Qapla)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setze das Projektverzeichnis als Basis für relative Pfade
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Alle C++-Dateien relativ zum Projektverzeichnis einbinden
file(GLOB_RECURSE SOURCES RELATIVE ${PROJECT_SOURCE_DIR} "*.cpp")
file(GLOB_RECURSE HEADERS RELATIVE ${PROJECT_SOURCE_DIR} "*.h")
list(FILTER SOURCES EXCLUDE REGEX "build/.*")  # Entfernt alle build/-Dateien
list(FILTER HEADERS EXCLUDE REGEX "build/.*")  # Entfernt alle build/-Header

# Include-Pfade setzen
foreach(HEADER ${HEADERS})
    get_filename_component(DIR ${HEADER} DIRECTORY)
    include_directories(${PROJECT_SOURCE_DIR}/${DIR})
endforeach()

# Debug-Build
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_COMPILER clang++)
    add_executable(Qapla ${SOURCES})
    target_compile_definitions(Qapla PRIVATE _DEBUG)
endif()

# Release-Build
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_CXX_COMPILER clang++)
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
    add_executable(Qapla ${SOURCES})
    target_compile_definitions(Qapla PRIVATE NDEBUG)
    set_target_properties(Qapla PROPERTIES COMPILE_FLAGS "-O3 -march=x86-64-v3 -flto -funroll-loops -fno-rtti")
endif()

# Custom Debug-Build
if(CMAKE_BUILD_TYPE STREQUAL "CustomDebug")
    set(CMAKE_CXX_COMPILER clang++)
    add_executable(Qapla ${SOURCES})
    target_compile_definitions(Qapla PRIVATE _DEBUG CUSTOM_DEFINE=1)
endif()

# Custom Release-Build
if(CMAKE_BUILD_TYPE STREQUAL "CustomRelease")
    set(CMAKE_CXX_COMPILER clang++)
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
    add_executable(Qapla ${SOURCES})
    target_compile_definitions(Qapla PRIVATE NDEBUG CUSTOM_DEFINE=1)
    set_target_properties(Qapla PROPERTIES COMPILE_FLAGS "-O3 -march=native -flto")
endif()

# Windows Release-Build (MinGW)
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++ -Wl,--allow-multiple-definition -pthread")

    add_executable(QaplaWin ${SOURCES})
    target_compile_definitions(QaplaWin PRIVATE NDEBUG)

    # Set Compiler-Flags for Windows
    set_target_properties(QaplaWin PROPERTIES COMPILE_FLAGS "-O3 -march=x86-64-v3 -flto -funroll-loops -fno-rtti -pthread")

    # Link Windows standard libraries
    target_link_libraries(QaplaWin kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)
endif()
