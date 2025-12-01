// dear imgui: Renderer Backend for Direct3D 3 (DirectX 3)
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'LPDIRECT3DTEXTURE2' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).

// CHANGELOG
//  2025-XX-XX: Initial Direct3D 3 backend implementation.

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_dx3.h"

// DirectX 3
#include <d3d.h>
#include <ddraw.h>

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

// Direct3D 3 data
struct ImGui_ImplD3D3_Data
{
    LPDIRECT3DDEVICE3       pd3dDevice;
    LPDIRECT3DEXECUTEBUFFER pExecuteBuffer;
    LPDIRECT3DVERTEXBUFFER  pVertexBuffer;
    LPDIRECT3DMATERIAL3     pMaterial;
    D3DMATERIALHANDLE       hMaterial;
    int                     VertexBufferSize;
    int                     IndexBufferSize;

    ImGui_ImplD3D3_Data() 
    { 
        memset((void*)this, 0, sizeof(*this)); 
        VertexBufferSize = 5000;
        IndexBufferSize = 10000;
    }
};

struct CUSTOMVERTEX
{
    D3DVALUE x, y, z;
    D3DCOLOR color;
    D3DVALUE u, v;
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

// Backend data stored in io.BackendRendererUserData
static ImGui_ImplD3D3_Data* ImGui_ImplD3D3_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplD3D3_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

// Functions
static void ImGui_ImplD3D3_SetupRenderState(ImDrawData* draw_data)
{
    ImGui_ImplD3D3_Data* bd = ImGui_ImplD3D3_GetBackendData();
    LPDIRECT3DDEVICE3 device = bd->pd3dDevice;

    // Setup viewport through Direct3D3 interface (not on device in D3D3)
    LPDIRECT3DVIEWPORT3 viewport = nullptr;
    LPDIRECT3D3 d3d3;
    if (device->GetDirect3D(&d3d3) == D3D_OK)
    {
        if (d3d3->CreateViewport(&viewport, nullptr) == D3D_OK)
        {
            device->AddViewport(viewport);
            
            D3DVIEWPORT2 vp;
            memset(&vp, 0, sizeof(D3DVIEWPORT2));
            vp.dwSize = sizeof(D3DVIEWPORT2);
            vp.dwX = 0;
            vp.dwY = 0;
            vp.dwWidth = (DWORD)draw_data->DisplaySize.x;
            vp.dwHeight = (DWORD)draw_data->DisplaySize.y;
            vp.dvClipX = -1.0f;
            vp.dvClipY = 1.0f;
            vp.dvClipWidth = 2.0f;
            vp.dvClipHeight = 2.0f;
            vp.dvMinZ = 0.0f;
            vp.dvMaxZ = 1.0f;
            
            viewport->SetViewport2(&vp);
            device->SetCurrentViewport(viewport);
            viewport->Release();
        }
        d3d3->Release();
    }

    // Setup render state: alpha-blending, no face culling, no depth testing
    device->SetRenderState(D3DRENDERSTATE_ZENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_CLIPPING, TRUE);
    device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
    device->SetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_TEXTUREADDRESS, D3DTADDRESS_CLAMP);
    device->SetRenderState(D3DRENDERSTATE_TEXTUREMAG, D3DFILTER_LINEAR);
    device->SetRenderState(D3DRENDERSTATE_TEXTUREMIN, D3DFILTER_LINEAR);

    // Setup orthographic projection matrix
    float L = draw_data->DisplayPos.x + 0.5f;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
    float T = draw_data->DisplayPos.y + 0.5f;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;
    
    D3DMATRIX mat_identity = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    D3DMATRIX mat_projection = {
        2.0f/(R-L),   0.0f,         0.0f, 0.0f,
        0.0f,         2.0f/(T-B),   0.0f, 0.0f,
        0.0f,         0.0f,         0.5f, 0.0f,
        (L+R)/(L-R),  (T+B)/(B-T),  0.5f, 1.0f
    };

    device->SetTransform(D3DTRANSFORMSTATE_WORLD, &mat_identity);
    device->SetTransform(D3DTRANSFORMSTATE_VIEW, &mat_identity);
    device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &mat_projection);
}

// Render function
void ImGui_ImplD3D3_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplD3D3_Data* bd = ImGui_ImplD3D3_GetBackendData();
    LPDIRECT3DDEVICE3 device = bd->pd3dDevice;

    // Catch up with texture updates
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplD3D3_UpdateTexture(tex);

    // Create and grow buffers if needed
    if (!bd->pVertexBuffer || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (bd->pVertexBuffer)
        {
            bd->pVertexBuffer->Release();
            bd->pVertexBuffer = nullptr;
        }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;

        D3DVERTEXBUFFERDESC vbDesc;
        memset(&vbDesc, 0, sizeof(D3DVERTEXBUFFERDESC));
        vbDesc.dwSize = sizeof(D3DVERTEXBUFFERDESC);
        vbDesc.dwCaps = D3DVBCAPS_WRITEONLY;
        vbDesc.dwFVF = D3DFVF_CUSTOMVERTEX;
        vbDesc.dwNumVertices = bd->VertexBufferSize;

        LPDIRECT3D3 d3d3;
        if (device->GetDirect3D(&d3d3) == D3D_OK)
        {
            d3d3->CreateVertexBuffer(&vbDesc, &bd->pVertexBuffer, 0, nullptr);
            d3d3->Release();
        }
    }

    // Setup desired D3D state
    ImGui_ImplD3D3_SetupRenderState(draw_data);

    // Copy and convert all vertices into vertex buffer
    CUSTOMVERTEX* vtx_dst;
    if (bd->pVertexBuffer->Lock(DDLOCK_WRITEONLY | DDLOCK_DISCARDCONTENTS, (void**)&vtx_dst, nullptr) == D3D_OK)
    {
        for (const ImDrawList* draw_list : draw_data->CmdLists)
        {
            const ImDrawVert* vtx_src = draw_list->VtxBuffer.Data;
            for (int i = 0; i < draw_list->VtxBuffer.Size; i++)
            {
                vtx_dst->x = vtx_src->pos.x;
                vtx_dst->y = vtx_src->pos.y;
                vtx_dst->z = 0.0f;
                vtx_dst->color = vtx_src->col;
                vtx_dst->u = vtx_src->uv.x;
                vtx_dst->v = vtx_src->uv.y;
                vtx_dst++;
                vtx_src++;
            }
        }
        bd->pVertexBuffer->Unlock();
    }

    // Render command lists
    int global_vtx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;

    for (const ImDrawList* draw_list : draw_data->CmdLists)
    {
        const ImDrawIdx* idx_buffer = draw_list->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            
            if (pcmd->UserCallback != nullptr)
            {
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplD3D3_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Bind texture
                LPDIRECT3DTEXTURE2 texture = (LPDIRECT3DTEXTURE2)pcmd->GetTexID();
                if (texture)
                    device->SetTexture(0, texture);

                // Draw primitives using DrawIndexedPrimitiveVB
                // Signature: D3DPRIMITIVETYPE, LPDIRECT3DVERTEXBUFFER, LPWORD indices, DWORD indexCount, DWORD flags
                device->DrawIndexedPrimitiveVB(
                    D3DPT_TRIANGLELIST,
                    bd->pVertexBuffer,
                    (LPWORD)(idx_buffer + pcmd->IdxOffset),
                    pcmd->ElemCount,
                    0
                );
            }
        }
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }
}

void ImGui_ImplD3D3_UpdateTexture(ImTextureData* tex)
{
    ImGui_ImplD3D3_Data* bd = ImGui_ImplD3D3_GetBackendData();

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create new texture
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        LPDIRECTDRAWSURFACE4 ddSurface = nullptr;
        LPDIRECT3DTEXTURE2 d3dTexture = nullptr;

        // Create texture surface using DirectDraw
        LPDIRECT3D3 d3d3;
        if (bd->pd3dDevice->GetDirect3D(&d3d3) == D3D_OK)
        {
            LPDIRECTDRAW4 ddraw;
            if (d3d3->QueryInterface(IID_IDirectDraw4, (void**)&ddraw) == DD_OK)
            {
                DDSURFACEDESC2 ddsd;
                memset(&ddsd, 0, sizeof(ddsd));
                ddsd.dwSize = sizeof(ddsd);
                ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
                ddsd.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
                ddsd.dwWidth = tex->Width;
                ddsd.dwHeight = tex->Height;
                ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
                ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
                ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
                ddsd.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
                ddsd.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
                ddsd.ddpfPixelFormat.dwBBitMask = 0x000000FF;
                ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;

                if (ddraw->CreateSurface(&ddsd, &ddSurface, nullptr) == DD_OK)
                {
                    if (ddSurface->QueryInterface(IID_IDirect3DTexture2, (void**)&d3dTexture) == D3D_OK)
                    {
                        // Lock and copy pixel data
                        DDSURFACEDESC2 lockDesc;
                        memset(&lockDesc, 0, sizeof(lockDesc));
                        lockDesc.dwSize = sizeof(lockDesc);
                        
                        if (ddSurface->Lock(nullptr, &lockDesc, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr) == DD_OK)
                        {
                            ImU32* src = (ImU32*)tex->GetPixels();
                            for (int y = 0; y < tex->Height; y++)
                            {
                                ImU32* dst = (ImU32*)((BYTE*)lockDesc.lpSurface + y * lockDesc.lPitch);
                                memcpy(dst, src + y * tex->Width, tex->Width * 4);
                            }
                            ddSurface->Unlock(nullptr);
                        }

                        tex->SetTexID((ImTextureID)d3dTexture);
                        tex->SetStatus(ImTextureStatus_OK);
                    }
                }
                ddraw->Release();
            }
            d3d3->Release();
        }
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        // Update texture regions
        LPDIRECT3DTEXTURE2 d3dTexture = (LPDIRECT3DTEXTURE2)tex->TexID;
        LPDIRECTDRAWSURFACE4 ddSurface = nullptr;
        
        if (d3dTexture->QueryInterface(IID_IDirectDrawSurface4, (void**)&ddSurface) == DD_OK)
        {
            DDSURFACEDESC2 lockDesc;
            memset(&lockDesc, 0, sizeof(lockDesc));
            lockDesc.dwSize = sizeof(lockDesc);
            
            RECT updateRect = { 
                (LONG)tex->UpdateRect.x, 
                (LONG)tex->UpdateRect.y, 
                (LONG)(tex->UpdateRect.x + tex->UpdateRect.w), 
                (LONG)(tex->UpdateRect.y + tex->UpdateRect.h) 
            };

            if (ddSurface->Lock(&updateRect, &lockDesc, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr) == DD_OK)
            {
                for (ImTextureRect& r : tex->Updates)
                {
                    ImU32* src = (ImU32*)tex->GetPixelsAt(r.x, r.y);
                    for (int y = 0; y < r.h; y++)
                    {
                        ImU32* dst = (ImU32*)((BYTE*)lockDesc.lpSurface + 
                                    (r.y - updateRect.top + y) * lockDesc.lPitch) + 
                                    (r.x - updateRect.left);
                        memcpy(dst, src + y * tex->Width, r.w * 4);
                    }
                }
                ddSurface->Unlock(nullptr);
            }
            ddSurface->Release();
        }
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        if (LPDIRECT3DTEXTURE2 d3dTexture = (LPDIRECT3DTEXTURE2)tex->TexID)
        {
            d3dTexture->Release();
            tex->SetTexID(ImTextureID_Invalid);
        }
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

bool ImGui_ImplD3D3_Init(IDirect3DDevice3* device)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplD3D3_Data* bd = IM_NEW(ImGui_ImplD3D3_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_d3d3";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = platform_io.Renderer_TextureMaxHeight = 256;

    bd->pd3dDevice = device;
    bd->pd3dDevice->AddRef();

    // Create material for D3D3
    D3DMATERIAL mat;
    memset(&mat, 0, sizeof(mat));
    mat.dwSize = sizeof(D3DMATERIAL);
    mat.diffuse.r = mat.diffuse.g = mat.diffuse.b = mat.diffuse.a = 1.0f;
    mat.ambient.r = mat.ambient.g = mat.ambient.b = mat.ambient.a = 1.0f;
    mat.dwRampSize = 1;

    LPDIRECT3D3 d3d3;
    if (device->GetDirect3D(&d3d3) == D3D_OK)
    {
        d3d3->CreateMaterial(&bd->pMaterial, nullptr);
        if (bd->pMaterial)
        {
            bd->pMaterial->SetMaterial(&mat);
            bd->pMaterial->GetHandle(device, &bd->hMaterial);
            device->SetLightState(D3DLIGHTSTATE_MATERIAL, bd->hMaterial);
        }
        d3d3->Release();
    }

    return true;
}

void ImGui_ImplD3D3_Shutdown()
{
    ImGui_ImplD3D3_Data* bd = ImGui_ImplD3D3_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ImGui_ImplD3D3_InvalidateDeviceObjects();

    if (bd->pMaterial)
        bd->pMaterial->Release();
    if (bd->pd3dDevice)
        bd->pd3dDevice->Release();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    platform_io.ClearRendererHandlers();
    IM_DELETE(bd);
}

bool ImGui_ImplD3D3_CreateDeviceObjects()
{
    ImGui_ImplD3D3_Data* bd = ImGui_ImplD3D3_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return false;
    return true;
}

void ImGui_ImplD3D3_InvalidateDeviceObjects()
{
    ImGui_ImplD3D3_Data* bd = ImGui_ImplD3D3_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return;

    // Destroy all textures
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplD3D3_UpdateTexture(tex);
        }

    if (bd->pVertexBuffer)
    {
        bd->pVertexBuffer->Release();
        bd->pVertexBuffer = nullptr;
    }
    if (bd->pExecuteBuffer)
    {
        bd->pExecuteBuffer->Release();
        bd->pExecuteBuffer = nullptr;
    }
}

void ImGui_ImplD3D3_NewFrame()
{
    ImGui_ImplD3D3_Data* bd = ImGui_ImplD3D3_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplD3D3_Init()?");
    IM_UNUSED(bd);
}

#endif // #ifndef IMGUI_DISABLE