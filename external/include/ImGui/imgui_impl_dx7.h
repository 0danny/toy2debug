// dear imgui: Renderer Backend for DirectX7
// This needs to be used along with a Platform Backend (for example Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'LPDIRECTDRAWSURFACE7' as texture identifier.
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16 bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).
//  [X] Renderer: IMGUI_USE_BGRA_PACKED_COLOR support.

// You can use unmodified imgui_impl_* files in your project. See examples folder for examples of using this.
// Prefer including the entire imgui repository into your project (either as a copy or as a submodule), and only build the backends you need.

#pragma once
#include "imgui.h"
#ifndef IMGUI_DISABLE

struct IDirect3DDevice7;

// Follow the Getting Started guide and check examples folder to learn about using backends.
IMGUI_IMPL_API bool     ImGui_ImplDX7_Init(IDirect3DDevice7* device);
IMGUI_IMPL_API void     ImGui_ImplDX7_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplDX7_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplDX7_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplDX7_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplDX7_InvalidateDeviceObjects();

// Advanced: use if you want to drive texture updates yourself by setting ImDrawData::Textures = nullptr.
IMGUI_IMPL_API void     ImGui_ImplDX7_UpdateTexture(ImTextureData* tex);

#endif // #ifndef IMGUI_DISABLE
