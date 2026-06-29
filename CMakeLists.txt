cmake_minimum_required(VERSION 3.20)

# Disable vcpkg MSBuild integration (prevents wildcard *.lib injection from vcpkg)
set(VCPKG_APPLOCAL_DEPS OFF)
set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")

project(SimpleMidiViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MFC (embedded in EXE, no MFC DLL dependency)
set(CMAKE_MFC_FLAG 1)

# Static CRT (/MT for Release, /MTd for Debug)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(SOURCES
    src/App.cpp
    src/MainFrame.cpp
    src/ScoreView.cpp
    src/MidiParser.cpp
    src/NoteListPanel.cpp
    src/SizerBar.cpp
    resources/resource.rc
)

set(HEADERS
    src/pch.h
    src/App.h
    src/MainFrame.h
    src/ScoreView.h
    src/MidiParser.h
    src/MidiDocument.h
    src/NoteListPanel.h
    src/AppMessages.h
    src/SizerBar.h
    resources/resource.h
)

add_executable(SimpleMidiViewer WIN32 ${SOURCES} ${HEADERS})

target_include_directories(SimpleMidiViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/resources
)

# Precompiled header
target_precompile_headers(SimpleMidiViewer PRIVATE src/pch.h)

# Unicode character set
target_compile_definitions(SimpleMidiViewer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WINDOWS
)

# Compiler options: warnings, disable incremental link in Release
target_compile_options(SimpleMidiViewer PRIVATE
    /W3
    /WX-
    /EHsc
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Debug>:/Od /Zi>
)

# Linker: system libraries needed by MFC static
target_link_libraries(SimpleMidiViewer PRIVATE
    comctl32
    winmm
    msimg32   # GradientFill (score header background)
)

# Linker subsystem
set_target_properties(SimpleMidiViewer PROPERTIES
    WIN32_EXECUTABLE TRUE
    LINK_FLAGS "/SUBSYSTEM:WINDOWS /NODEFAULTLIB:msvcrt.lib"
)
