cmake_minimum_required(VERSION 3.5)
project(cimgui-sokol-starterkit)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Linux -pthread shenanigans
if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

#=== LIBRARY: cimgui + Dear ImGui
add_library(cimgui STATIC
    cimgui/cimgui.cpp
    cimgui/cimgui.h
    cimgui/imgui/imgui.cpp
    cimgui/imgui/imgui.h
    cimgui/imgui/imgui_widgets.cpp
    cimgui/imgui/imgui_draw.cpp
    cimgui/imgui/imgui_tables.cpp
    cimgui/imgui/imgui_demo.cpp)
target_include_directories(cimgui INTERFACE cimgui)

#=== LIBRARY: sokol
# add headers to the the file list because they are useful to have in IDEs
set(SOKOL_HEADERS
    sokol/sokol_gfx.h
    sokol/sokol_app.h
    sokol/sokol_imgui.h
    sokol/sokol_glue.h)
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    add_library(sokol STATIC sokol/sokol.c ${SOKOL_HEADERS})
    target_compile_options(sokol PRIVATE -x objective-c)
    target_link_libraries(sokol PUBLIC
        "-framework QuartzCore"
        "-framework Cocoa"
        "-framework MetalKit"
        "-framework Metal")
else()
    add_library(sokol STATIC sokol/sokol.c ${SOKOL_HEADERS})
    if (CMAKE_SYSTEM_NAME STREQUAL Linux)
        target_link_libraries(sokol INTERFACE X11 Xi Xcursor GL dl m)
        target_link_libraries(sokol PUBLIC Threads::Threads)
    endif()
endif()
target_link_libraries(sokol PUBLIC cimgui)
target_include_directories(sokol INTERFACE sokol)

#file(GLOB_RECURSE sources
#        "${CMAKE_SOURCE_DIR}/**/*.c"
#        "${CMAKE_SOURCE_DIR}/**/*.cpp"
#        )
#add_definitions(-DSOME_DEFINITION)

#=== EXECUTABLE: afaire
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_executable(afaire WIN32 afaire.c)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT afaire)
else()
    add_executable(afaire afaire.c)
endif()
target_link_libraries(afaire sokol)

# Emscripten-specific linker options
if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    # use our own minimal shell.html
    target_link_options(afaire PRIVATE --shell-file ../sokol/shell.html)
    # link with WebGL2
    target_link_options(afaire PRIVATE -sUSE_WEBGL2=1)
    # WASM+JS size optimizations
    target_link_options(afaire PRIVATE -sNO_FILESYSTEM=1 -sASSERTIONS=0 -sMALLOC=emmalloc --closure=1)
endif()

# explicitly strip dead code
if (CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    target_link_options(afaire PRIVATE LINKER:-dead_strip)
endif()

# this hack removes the xxx-CMakeForceLinker.cxx dummy file
set_target_properties(afaire PROPERTIES LINKER_LANGUAGE C)
