set(PROJECT_NAME "toy2debug")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_minimum_required(VERSION 3.20.0)
project(${PROJECT_NAME} C CXX RC)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include)
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
    src/Renderer.cpp
    src/Renderer.cpp

    src/Mapper.cpp
    src/Mapper.hpp

    src/Tabs/Tab.hpp

    src/Tabs/MainTab.cpp
    src/Tabs/MainTab.hpp

    src/Tabs/DebugTab.cpp
    src/Tabs/DebugTab.hpp

    src/Settings.cpp
    src/Settings.hpp

    src/RenderAPI/DirectX6.cpp
    src/RenderAPI/DirectX6.hpp

    src/RenderAPI/DirectX9.cpp
    src/RenderAPI/DirectX9.hpp

    # DirectX6 Hooks
    src/Hooks/DX6/DeviceHooks.cpp
    src/Hooks/DX6/DeviceHooks.hpp
    # End DirectX6 Hooks

    # DirectX9 Hooks
    src/Hooks/DX9/DeviceHooks.cpp
    src/Hooks/DX9/DeviceHooks.hpp

    src/Hooks/DX9/LightsHooks.cpp
    src/Hooks/DX9/LightsHooks.hpp

    src/Hooks/DX9/VBHooks.cpp
    src/Hooks/DX9/VBHooks.hpp

    src/Hooks/DX9/DrawHooks.cpp
    src/Hooks/DX9/DrawHooks.hpp

    src/Hooks/DX9/PipelineHooks.cpp
    src/Hooks/DX9/PipelineHooks.hpp

    src/Hooks/DX9/GlueHooks.cpp
    src/Hooks/DX9/GlueHooks.hpp

    src/Hooks/DX9/TexMatHooks.cpp
    src/Hooks/DX9/TexMatHooks.hpp

    src/Hooks/DX9/StubHooks.hpp
    # End DirectX9 Hooks

    src/Hooks/CommonHooks.hpp
    src/Hooks/RendererCommon.hpp

    src/Hooks/WindowHooks.cpp
    src/Hooks/WindowHooks.hpp

    src/ToyTypes.hpp
    src/ToyTypesDX9.hpp

    src/Utils.hpp

    src/Main.cpp
)

# Build the loader
add_executable(loader WIN32
    ${IMGUI_SOURCES}
    ${LOADER_SOURCES}
    ${RESOURCE_FILES}
)

target_link_directories(loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/libs)
target_link_libraries(loader PRIVATE d3d9 ddraw dxguid msimg32 MinHook.x32)
target_compile_definitions(loader PRIVATE MH_STATIC)