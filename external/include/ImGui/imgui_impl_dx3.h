// dear imgui: Renderer Backend for Direct3D 3 (DirectX 3)
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'LPDIRECT3DTEXTURE2' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
//  - FAQ                 https://dearimgui.com/faq
//  - Getting Started     https://dearimgui.com/getting-started
//  - Documentation       https://dearimgui.com/docs (same as your local docs/ folder).
//  - Introduction, links and more at the top of imgui.cpp

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

#ifndef IMGUI_DISABLE

struct IDirect3DDevice3;

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool     ImGui_ImplD3D3_Init(IDirect3DDevice3* device);
IMGUI_IMPL_API void     ImGui_ImplD3D3_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplD3D3_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplD3D3_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplD3D3_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplD3D3_InvalidateDeviceObjects();

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates.
IMGUI_IMPL_API void     ImGui_ImplD3D3_UpdateTexture(ImTextureData* tex);

#endif // #ifndef IMGUI_DISABLE