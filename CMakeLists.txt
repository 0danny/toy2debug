set(PROJECT_NAME "toy2debug")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_minimum_required(VERSION 3.20.0)
project(${PROJECT_NAME} C CXX RC)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/ToyDebug)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(IMGUI_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/external/include/ImGui)

set(RESOURCE_FILES
    resources/app.rc
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
    src/Injector.hpp
    src/Injector.cpp
    src/InjectorUtils.hpp

    src/Renderer.cpp
    src/Renderer.cpp

    src/Mapper.cpp
    src/Mapper.hpp

    src/Tabs/Tab.hpp

    src/Tabs/MainTab.cpp
    src/Tabs/MainTab.hpp

    src/Tabs/DebugTab.cpp
    src/Tabs/DebugTab.hpp

    src/Settings.hpp

    src/RenderAPI/DirectX3.cpp
    src/RenderAPI/DirectX3.hpp

    src/RenderAPI/DirectX9.cpp
    src/RenderAPI/DirectX9.hpp

    src/Hooks/DX3/DeviceHooks.cpp
    src/Hooks/DX3/DeviceHooks.hpp

    src/Hooks/CommonHooks.hpp
    src/Hooks/RendererCommon.hpp

    src/Hooks/WindowHooks.cpp
    src/Hooks/WindowHooks.hpp

    src/Main.cpp
)

# Build the loader
add_executable(loader WIN32
    ${IMGUI_SOURCES}
    ${LOADER_SOURCES}
    ${RESOURCE_FILES}
)

target_link_directories(loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/libs)
target_link_libraries(loader PRIVATE d3d9 ddraw dxguid dwmapi MinHook.x32)
target_compile_definitions(loader PRIVATE MH_STATIC)

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

# # Build the toydebug dll
# add_library(toydebug SHARED
#     ${TOYDEBUG_SOURCES}
#     ${IMGUI_SOURCES}
# )

# target_link_directories(toydebug PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/libs)
# target_link_libraries(toydebug PRIVATE MinHook.x32 d3d9 ddraw dxguid msimg32)
# target_compile_definitions(toydebug PRIVATE MH_STATIC)
