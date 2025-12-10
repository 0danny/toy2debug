set(PROJECT_NAME "toy2debug")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_minimum_required(VERSION 3.20.0)
project(${PROJECT_NAME} C CXX RC)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/ToyDebug)

set(IMGUI_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/external/include/ImGui)

# Build ImGui in statically
set(IMGUI_SOURCES
    ${IMGUI_FOLDER}/imconfig.h

    ${IMGUI_FOLDER}/imgui.cpp
    ${IMGUI_FOLDER}/imgui.h

    ${IMGUI_FOLDER}/imgui_demo.cpp
    ${IMGUI_FOLDER}/imgui_draw.cpp
    
    ${IMGUI_FOLDER}/imgui_impl_dx9.cpp
    ${IMGUI_FOLDER}/imgui_impl_dx9.h

    ${IMGUI_FOLDER}/imgui_impl_win32.cpp
    ${IMGUI_FOLDER}/imgui_impl_win32.h
    ${IMGUI_FOLDER}/imgui_wndproc_wrapper.h

    ${IMGUI_FOLDER}/imgui_tables.cpp
    ${IMGUI_FOLDER}/imgui_widgets.cpp

    ${IMGUI_FOLDER}/imgui_internal.h
    ${IMGUI_FOLDER}/imstb_rectpack.h
    ${IMGUI_FOLDER}/imstb_textedit.h
    ${IMGUI_FOLDER}/imstb_truetype.h
)

set(LOADER_SOURCES
    src/Loader/Injector.hpp
    src/Loader/Injector.cpp
    src/Loader/InjectorUtils.hpp

    src/Loader/Renderer.cpp
    src/Loader/Renderer.cpp

    src/Loader/Mapper.cpp
    src/Loader/Mapper.hpp

    src/Loader/Tabs/Tab.hpp

    src/Loader/Main.cpp
)

# Build the loader
add_executable(loader WIN32
    ${IMGUI_SOURCES}
    ${LOADER_SOURCES}
)

target_link_libraries(loader PRIVATE d3d9 ddraw)

set(TOYDEBUG_SOURCES
    src/ToyDebug/Main.cpp

    src/ToyDebug/Renderer/Renderer.cpp
    src/ToyDebug/Renderer/Renderer.hpp
    src/ToyDebug/Renderer/RendererCommon.hpp

    src/ToyDebug/Renderer/Hooks/DeviceHooks.cpp
    src/ToyDebug/Renderer/Hooks/DeviceHooks.hpp

    src/ToyDebug/Renderer/Hooks/LightsHooks.cpp
    src/ToyDebug/Renderer/Hooks/LightsHooks.hpp

    src/ToyDebug/Renderer/Hooks/VBHooks.cpp
    src/ToyDebug/Renderer/Hooks/VBHooks.hpp

    src/ToyDebug/Renderer/Hooks/DrawHooks.cpp
    src/ToyDebug/Renderer/Hooks/DrawHooks.hpp

    src/ToyDebug/Renderer/Hooks/PipelineHooks.cpp
    src/ToyDebug/Renderer/Hooks/PipelineHooks.hpp

    src/ToyDebug/Renderer/Hooks/GlueHooks.cpp
    src/ToyDebug/Renderer/Hooks/GlueHooks.hpp

    src/ToyDebug/Renderer/Hooks/TexMatHooks.cpp
    src/ToyDebug/Renderer/Hooks/TexMatHooks.hpp

    src/ToyDebug/Renderer/Hooks/MiscHooks.cpp
    src/ToyDebug/Renderer/Hooks/MiscHooks.hpp

    src/ToyDebug/Renderer/Hooks/StubHooks.hpp

    src/ToyDebug/ToyTypes.hpp
    src/ToyDebug/Hook.hpp
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
