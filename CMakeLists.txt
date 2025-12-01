set(PROJECT_NAME "toy2debug")

# Need 32-bit dll and exe for injecting into Toy2
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_minimum_required(VERSION 3.20.0)
project(${PROJECT_NAME} C CXX RC)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include)

set(IMGUI_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/external/include/ImGui)

# Build the loader
add_executable(loader 
    src/Loader/Injector.hpp
    src/Loader/Injector.cpp

    src/Loader/Main.cpp
)

# Build ImGui in statically
set(IMGUI_SOURCES
    ${IMGUI_FOLDER}/imconfig.h

    ${IMGUI_FOLDER}/imgui.cpp
    ${IMGUI_FOLDER}/imgui.h

    ${IMGUI_FOLDER}/imgui_demo.cpp
    ${IMGUI_FOLDER}/imgui_draw.cpp
    
    ${IMGUI_FOLDER}/imgui_impl_dx9.cpp
    ${IMGUI_FOLDER}/imgui_impl_dx9.h

    ${IMGUI_FOLDER}/imgui_impl_dx3.cpp
    ${IMGUI_FOLDER}/imgui_impl_dx3.h

    ${IMGUI_FOLDER}/imgui_impl_win32.cpp
    ${IMGUI_FOLDER}/imgui_impl_win32.h

    ${IMGUI_FOLDER}/imgui_tables.cpp
    ${IMGUI_FOLDER}/imgui_widgets.cpp

    ${IMGUI_FOLDER}/imgui_internal.h
    ${IMGUI_FOLDER}/imstb_rectpack.h
    ${IMGUI_FOLDER}/imstb_textedit.h
    ${IMGUI_FOLDER}/imstb_truetype.h
)

# ToyDebug Sources
set(TOYDEBUG_SOURCES
    src/ToyDebug/Main.cpp

    src/ToyDebug/Renderer.cpp
    src/ToyDebug/Renderer.hpp

    src/ToyDebug/Utils.hpp
)

# Build the toydebug dll
add_library(toydebug SHARED
    ${TOYDEBUG_SOURCES}
    ${IMGUI_SOURCES}
)

target_link_directories(toydebug PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/libs)
target_link_libraries(toydebug PRIVATE MinHook.x32 d3d9 ddraw dxguid msimg32)
target_compile_definitions(toydebug PRIVATE MH_STATIC)
