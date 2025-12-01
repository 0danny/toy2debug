// dear imgui: Renderer Backend for DirectX7
// This needs to be used along with a Platform Backend (for example Win32)

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_dx7.h"

// DirectX 7
#include <ddraw.h>
#include <d3d.h>

// Clang or GCC warnings with very pedantic settings
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

// DirectX data
struct ImGui_ImplDX7_Data
{
    LPDIRECT3DDEVICE7       pd3dDevice;
    LPDIRECTDRAW7           pDD;
    int                     VertexBufferSize;
    int                     IndexBufferSize;
    struct CUSTOMVERTEX*    pVB;
    ImDrawIdx*              pIB;
    bool                    HasRgbaSupport; // not really used, kept for parity with DX9 backend

    ImGui_ImplDX7_Data()
    {
        memset(this, 0, sizeof(*this));
        VertexBufferSize = 0;
        IndexBufferSize = 0;
        HasRgbaSupport = false;
    }
};

struct CUSTOMVERTEX
{
    float    pos[3];
    D3DCOLOR col;
    float    uv[2];
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_COL_TO_DX7_ARGB(_COL)     (_COL)
#else
#define IMGUI_COL_TO_DX7_ARGB(_COL)     (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))
#endif

static ImGui_ImplDX7_Data* ImGui_ImplDX7_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX7_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

// Backup subset of device state that we change
struct ImGui_ImplDX7_StateBackup
{
    D3DVIEWPORT7           viewport;
    D3DMATRIX              world;
    D3DMATRIX              view;
    D3DMATRIX              projection;

    DWORD                  rs_fillmode;
    DWORD                  rs_shademode;
    DWORD                  rs_zenable;
    DWORD                  rs_zwriteenable;
    DWORD                  rs_alphatestenable;
    DWORD                  rs_cullmode;
    DWORD                  rs_alphablendenable;
    DWORD                  rs_srcblend;
    DWORD                  rs_destblend;
    DWORD                  rs_fogenable;
    DWORD                  rs_specularenable;
    DWORD                  rs_stencilenable;
    DWORD                  rs_clipping;
    DWORD                  rs_lighting;
    DWORD                  rs_alpharef;
    DWORD                  rs_alphafunc;

    DWORD                  tss0_colorop;
    DWORD                  tss0_colorarg1;
    DWORD                  tss0_colorarg2;
    DWORD                  tss0_alphaop;
    DWORD                  tss0_alphaarg1;
    DWORD                  tss0_alphaarg2;
    DWORD                  tss0_minfilter;
    DWORD                  tss0_magfilter;
    DWORD                  tss0_addressu;
    DWORD                  tss0_addressv;
    DWORD                  tss1_colorop;
    DWORD                  tss1_alphaop;

    LPDIRECTDRAWSURFACE7   texture0;
};

static void ImGui_ImplDX7_BackupState(ImGui_ImplDX7_StateBackup* backup)
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    LPDIRECT3DDEVICE7 device = bd->pd3dDevice;

    device->GetViewport(&backup->viewport);
    device->GetTransform(D3DTRANSFORMSTATE_WORLD,       &backup->world);
    device->GetTransform(D3DTRANSFORMSTATE_VIEW,        &backup->view);
    device->GetTransform(D3DTRANSFORMSTATE_PROJECTION,  &backup->projection);

    device->GetRenderState(D3DRENDERSTATE_FILLMODE,         &backup->rs_fillmode);
    device->GetRenderState(D3DRENDERSTATE_SHADEMODE,        &backup->rs_shademode);
    device->GetRenderState(D3DRENDERSTATE_ZENABLE,          &backup->rs_zenable);
    device->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE,     &backup->rs_zwriteenable);
    device->GetRenderState(D3DRENDERSTATE_ALPHATESTENABLE,  &backup->rs_alphatestenable);
    device->GetRenderState(D3DRENDERSTATE_CULLMODE,         &backup->rs_cullmode);
    device->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &backup->rs_alphablendenable);
    device->GetRenderState(D3DRENDERSTATE_SRCBLEND,         &backup->rs_srcblend);
    device->GetRenderState(D3DRENDERSTATE_DESTBLEND,        &backup->rs_destblend);
    device->GetRenderState(D3DRENDERSTATE_FOGENABLE,        &backup->rs_fogenable);
    device->GetRenderState(D3DRENDERSTATE_SPECULARENABLE,   &backup->rs_specularenable);
    device->GetRenderState(D3DRENDERSTATE_STENCILENABLE,    &backup->rs_stencilenable);
    device->GetRenderState(D3DRENDERSTATE_CLIPPING,         &backup->rs_clipping);
    device->GetRenderState(D3DRENDERSTATE_LIGHTING,         &backup->rs_lighting);
    device->GetRenderState(D3DRENDERSTATE_ALPHAREF,         &backup->rs_alpharef);
    device->GetRenderState(D3DRENDERSTATE_ALPHAFUNC,        &backup->rs_alphafunc);

    device->GetTextureStageState(0, D3DTSS_COLOROP,        &backup->tss0_colorop);
    device->GetTextureStageState(0, D3DTSS_COLORARG1,      &backup->tss0_colorarg1);
    device->GetTextureStageState(0, D3DTSS_COLORARG2,      &backup->tss0_colorarg2);
    device->GetTextureStageState(0, D3DTSS_ALPHAOP,        &backup->tss0_alphaop);
    device->GetTextureStageState(0, D3DTSS_ALPHAARG1,      &backup->tss0_alphaarg1);
    device->GetTextureStageState(0, D3DTSS_ALPHAARG2,      &backup->tss0_alphaarg2);
    device->GetTextureStageState(0, D3DTSS_MINFILTER,      &backup->tss0_minfilter);
    device->GetTextureStageState(0, D3DTSS_MAGFILTER,      &backup->tss0_magfilter);
    device->GetTextureStageState(0, D3DTSS_ADDRESSU,       &backup->tss0_addressu);
    device->GetTextureStageState(0, D3DTSS_ADDRESSV,       &backup->tss0_addressv);

    device->GetTextureStageState(1, D3DTSS_COLOROP,        &backup->tss1_colorop);
    device->GetTextureStageState(1, D3DTSS_ALPHAOP,        &backup->tss1_alphaop);

    backup->texture0 = nullptr;
    device->GetTexture(0, &backup->texture0); // adds ref if non null
}

static void ImGui_ImplDX7_RestoreState(const ImGui_ImplDX7_StateBackup* backup)
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    LPDIRECT3DDEVICE7 device = bd->pd3dDevice;

    device->SetViewport((D3DVIEWPORT7*)&backup->viewport);
    device->SetTransform(D3DTRANSFORMSTATE_WORLD,       (D3DMATRIX*)&backup->world);
    device->SetTransform(D3DTRANSFORMSTATE_VIEW,        (D3DMATRIX*)&backup->view);
    device->SetTransform(D3DTRANSFORMSTATE_PROJECTION,  (D3DMATRIX*)&backup->projection);

    device->SetRenderState(D3DRENDERSTATE_FILLMODE,         backup->rs_fillmode);
    device->SetRenderState(D3DRENDERSTATE_SHADEMODE,        backup->rs_shademode);
    device->SetRenderState(D3DRENDERSTATE_ZENABLE,          backup->rs_zenable);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,     backup->rs_zwriteenable);
    device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE,  backup->rs_alphatestenable);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE,         backup->rs_cullmode);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, backup->rs_alphablendenable);
    device->SetRenderState(D3DRENDERSTATE_SRCBLEND,         backup->rs_srcblend);
    device->SetRenderState(D3DRENDERSTATE_DESTBLEND,        backup->rs_destblend);
    device->SetRenderState(D3DRENDERSTATE_FOGENABLE,        backup->rs_fogenable);
    device->SetRenderState(D3DRENDERSTATE_SPECULARENABLE,   backup->rs_specularenable);
    device->SetRenderState(D3DRENDERSTATE_STENCILENABLE,    backup->rs_stencilenable);
    device->SetRenderState(D3DRENDERSTATE_CLIPPING,         backup->rs_clipping);
    device->SetRenderState(D3DRENDERSTATE_LIGHTING,         backup->rs_lighting);
    device->SetRenderState(D3DRENDERSTATE_ALPHAREF,         backup->rs_alpharef);
    device->SetRenderState(D3DRENDERSTATE_ALPHAFUNC,        backup->rs_alphafunc);

    device->SetTextureStageState(0, D3DTSS_COLOROP,        backup->tss0_colorop);
    device->SetTextureStageState(0, D3DTSS_COLORARG1,      backup->tss0_colorarg1);
    device->SetTextureStageState(0, D3DTSS_COLORARG2,      backup->tss0_colorarg2);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP,        backup->tss0_alphaop);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1,      backup->tss0_alphaarg1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2,      backup->tss0_alphaarg2);
    device->SetTextureStageState(0, D3DTSS_MINFILTER,      backup->tss0_minfilter);
    device->SetTextureStageState(0, D3DTSS_MAGFILTER,      backup->tss0_magfilter);
    device->SetTextureStageState(0, D3DTSS_ADDRESSU,       backup->tss0_addressu);
    device->SetTextureStageState(0, D3DTSS_ADDRESSV,       backup->tss0_addressv);

    device->SetTextureStageState(1, D3DTSS_COLOROP,        backup->tss1_colorop);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP,        backup->tss1_alphaop);

    device->SetTexture(0, backup->texture0);
    if (backup->texture0)
        backup->texture0->Release();
}

// Render state setup
static void ImGui_ImplDX7_SetupRenderState(ImDrawData* draw_data)
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    LPDIRECT3DDEVICE7 device = bd->pd3dDevice;

    // Setup viewport
    D3DVIEWPORT7 vp;
    memset(&vp, 0, sizeof(vp));
    vp.dwX      = 0;
    vp.dwY      = 0;
    vp.dwWidth  = (DWORD)draw_data->DisplaySize.x;
    vp.dwHeight = (DWORD)draw_data->DisplaySize.y;
    vp.dvMinZ   = 0.0f;
    vp.dvMaxZ   = 1.0f;
    device->SetViewport(&vp);

    // Fixed function pipeline setup
    device->SetRenderState(D3DRENDERSTATE_FILLMODE,         D3DFILL_SOLID);
    device->SetRenderState(D3DRENDERSTATE_SHADEMODE,        D3DSHADE_GOURAUD);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,     FALSE);
    device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE,  FALSE);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE,         D3DCULL_NONE);
    device->SetRenderState(D3DRENDERSTATE_ZENABLE,          FALSE);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRENDERSTATE_SRCBLEND,         D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRENDERSTATE_DESTBLEND,        D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRENDERSTATE_FOGENABLE,        FALSE);
    device->SetRenderState(D3DRENDERSTATE_SPECULARENABLE,   FALSE);
    device->SetRenderState(D3DRENDERSTATE_STENCILENABLE,    FALSE);
    device->SetRenderState(D3DRENDERSTATE_CLIPPING,         TRUE);
    device->SetRenderState(D3DRENDERSTATE_LIGHTING,         FALSE);
    device->SetRenderState(D3DRENDERSTATE_ALPHAREF,         0);
    device->SetRenderState(D3DRENDERSTATE_ALPHAFUNC,        D3DCMP_ALWAYS);

    device->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);

    // Filtering and addressing
    device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DFILTER_LINEAR);
    device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DFILTER_LINEAR);
    device->SetTextureStageState(0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP);
    device->SetTextureStageState(0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP);

    // Orthographic projection
    float L = draw_data->DisplayPos.x + 0.5f;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
    float T = draw_data->DisplayPos.y + 0.5f;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;

    D3DMATRIX mat_identity;
    memset(&mat_identity, 0, sizeof(mat_identity));
    mat_identity._11 = 1.0f;
    mat_identity._22 = 1.0f;
    mat_identity._33 = 1.0f;
    mat_identity._44 = 1.0f;

    D3DMATRIX mat_projection;
    memset(&mat_projection, 0, sizeof(mat_projection));
    mat_projection._11 =  2.0f / (R - L);
    mat_projection._22 =  2.0f / (T - B);
    mat_projection._33 =  0.5f;
    mat_projection._41 = (L + R) / (L - R);
    mat_projection._42 = (T + B) / (B - T);
    mat_projection._43 =  0.5f;
    mat_projection._44 =  1.0f;

    device->SetTransform(D3DTRANSFORMSTATE_WORLD,      &mat_identity);
    device->SetTransform(D3DTRANSFORMSTATE_VIEW,       &mat_identity);
    device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &mat_projection);
}

// Convert RGBA32 to ARGB32 stored in DirectDraw surface
static void ImGui_ImplDX7_CopyTextureRegion(bool tex_use_colors, const ImU32* src, int src_pitch, ImU32* dst, int dst_pitch, int w, int h)
{
#ifndef IMGUI_USE_BGRA_PACKED_COLOR
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    const bool convert_rgba_to_bgra = (!bd->HasRgbaSupport && tex_use_colors);
#else
    const bool convert_rgba_to_bgra = false;
    IM_UNUSED(tex_use_colors);
#endif

    for (int y = 0; y < h; y++)
    {
        const ImU32* src_p = (const ImU32*)((const unsigned char*)src + src_pitch * y);
        ImU32*       dst_p = (ImU32*)((unsigned char*)dst + dst_pitch * y);
        if (convert_rgba_to_bgra)
        {
            for (int x = 0; x < w; x++, src_p++, dst_p++)
                *dst_p = IMGUI_COL_TO_DX7_ARGB(*src_p);
        }
        else
        {
            memcpy(dst_p, src_p, w * 4);
        }
    }
}

// Texture management using ImTextureData and DirectDraw surfaces
void ImGui_ImplDX7_UpdateTexture(ImTextureData* tex)
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    if (!bd || !bd->pDD)
        return;

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        DDSURFACEDESC2 ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize                 = sizeof(ddsd);
        ddsd.dwFlags                = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
        ddsd.ddsCaps.dwCaps         = DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE;
        ddsd.dwWidth                = tex->Width;
        ddsd.dwHeight               = tex->Height;

        // 32 bit ARGB
        ddsd.ddpfPixelFormat.dwSize           = sizeof(ddsd.ddpfPixelFormat);
        ddsd.ddpfPixelFormat.dwFlags          = DDPF_RGB | DDPF_ALPHAPIXELS;
        ddsd.ddpfPixelFormat.dwRGBBitCount    = 32;
        ddsd.ddpfPixelFormat.dwRBitMask       = 0x00ff0000;
        ddsd.ddpfPixelFormat.dwGBitMask       = 0x0000ff00;
        ddsd.ddpfPixelFormat.dwBBitMask       = 0x000000ff;
        ddsd.ddpfPixelFormat.dwRGBAlphaBitMask= 0xff000000;

        LPDIRECTDRAWSURFACE7 surf = nullptr;
        HRESULT hr = bd->pDD->CreateSurface(&ddsd, &surf, nullptr);
        if (FAILED(hr) || !surf)
        {
            IM_ASSERT(0 && "ImGui_implDX7_UpdateTexture: CreateSurface failed");
            return;
        }

        DDSURFACEDESC2 lock_desc;
        memset(&lock_desc, 0, sizeof(lock_desc));
        lock_desc.dwSize = sizeof(lock_desc);
        if (surf->Lock(nullptr, &lock_desc, DDLOCK_WAIT, nullptr) == DD_OK)
        {
            ImGui_ImplDX7_CopyTextureRegion(tex->UseColors,
                                            (const ImU32*)tex->GetPixels(), tex->Width * 4,
                                            (ImU32*)lock_desc.lpSurface, (int)lock_desc.lPitch,
                                            tex->Width, tex->Height);
            surf->Unlock(nullptr);
        }

        tex->SetTexID((ImTextureID)(intptr_t)surf);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        LPDIRECTDRAWSURFACE7 surf = (LPDIRECTDRAWSURFACE7)(intptr_t)tex->TexID;
        if (!surf)
            return;

        RECT update_rect;
        update_rect.left   = (LONG)tex->UpdateRect.x;
        update_rect.top    = (LONG)tex->UpdateRect.y;
        update_rect.right  = (LONG)(tex->UpdateRect.x + tex->UpdateRect.w);
        update_rect.bottom = (LONG)(tex->UpdateRect.y + tex->UpdateRect.h);

        DDSURFACEDESC2 lock_desc;
        memset(&lock_desc, 0, sizeof(lock_desc));
        lock_desc.dwSize = sizeof(lock_desc);

        if (surf->Lock(&update_rect, &lock_desc, DDLOCK_WAIT, nullptr) == DD_OK)
        {
            ImU32* base_dst = (ImU32*)lock_desc.lpSurface;
            int pitch_in_pixels = (int)(lock_desc.lPitch / 4);

            for (ImTextureRect& r : tex->Updates)
            {
                ImGui_ImplDX7_CopyTextureRegion(
                    tex->UseColors,
                    (const ImU32*)tex->GetPixelsAt(r.x, r.y), tex->Width * 4,
                    base_dst + (r.x - update_rect.left) + (r.y - update_rect.top) * pitch_in_pixels,
                    (int)lock_desc.lPitch,
                    r.w, r.h);
            }
            surf->Unlock(&update_rect);
        }
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        if (LPDIRECTDRAWSURFACE7 surf = (LPDIRECTDRAWSURFACE7)tex->TexID)
        {
            IM_ASSERT(tex->TexID == (ImTextureID)(intptr_t)surf);
            surf->Release();
            tex->SetTexID(ImTextureID_Invalid);
        }
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

// Render function
void ImGui_ImplDX7_RenderDrawData(ImDrawData* draw_data)
{
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    LPDIRECT3DDEVICE7 device = bd->pd3dDevice;

    // Apply any pending texture updates
    if (draw_data->Textures != nullptr)
    {
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplDX7_UpdateTexture(tex);
    }

    // Allocate or grow our system memory buffers
    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        delete[] bd->pVB;
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        bd->pVB = new CUSTOMVERTEX[bd->VertexBufferSize];
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
    {
        delete[] bd->pIB;
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        bd->pIB = new ImDrawIdx[bd->IndexBufferSize];
    }

    // Backup device state
    ImGui_ImplDX7_StateBackup old;
    ImGui_ImplDX7_BackupState(&old);

    // Copy vertices and indices in a single contiguous buffer
    CUSTOMVERTEX* vtx_dst = bd->pVB;
    ImDrawIdx* idx_dst    = bd->pIB;
    int global_vtx_offset = 0;
    int global_idx_offset = 0;

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];

        const ImDrawVert* vtx_src = draw_list->VtxBuffer.Data;
        for (int i = 0; i < draw_list->VtxBuffer.Size; i++)
        {
            vtx_dst->pos[0] = vtx_src->pos.x;
            vtx_dst->pos[1] = vtx_src->pos.y;
            vtx_dst->pos[2] = 0.0f;
            vtx_dst->col    = IMGUI_COL_TO_DX7_ARGB(vtx_src->col);
            vtx_dst->uv[0]  = vtx_src->uv.x;
            vtx_dst->uv[1]  = vtx_src->uv.y;
            vtx_dst++;
            vtx_src++;
        }

        // Copy indices first, they are still local to this draw list for now
        memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst         += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
        global_idx_offset += draw_list->IdxBuffer.Size;
    }

    // Adjust indices to become global, honouring VtxOffset
    global_vtx_offset = 0;
    global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];

        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            int idx_start = pcmd->IdxOffset + global_idx_offset;
            int idx_end   = idx_start + pcmd->ElemCount;
            ImDrawIdx base = (ImDrawIdx)(pcmd->VtxOffset + global_vtx_offset);
            for (int i = idx_start; i < idx_end; i++)
                bd->pIB[i] = (ImDrawIdx)(bd->pIB[i] + base);
        }

        global_vtx_offset += draw_list->VtxBuffer.Size;
        global_idx_offset += draw_list->IdxBuffer.Size;
    }

    // Setup device render state
    ImGui_ImplDX7_SetupRenderState(draw_data);

    // Render command lists
    ImVec2 clip_off   = draw_data->DisplayPos;
    global_idx_offset = 0;

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];

            if (pcmd->UserCallback != nullptr)
            {
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplDX7_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(draw_list, pcmd);
                continue;
            }

            // Basic coarse clip using command rect against display, since D3D7 lacks native scissor
            ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
            ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
            if (clip_max.x <= 0.0f || clip_max.y <= 0.0f || clip_min.x >= draw_data->DisplaySize.x || clip_min.y >= draw_data->DisplaySize.y)
                continue;

            LPDIRECTDRAWSURFACE7 texture = (LPDIRECTDRAWSURFACE7)(intptr_t)pcmd->GetTexID();
            device->SetTexture(0, texture);

            const ImDrawIdx* idx_start = bd->pIB + pcmd->IdxOffset + global_idx_offset;
            device->DrawIndexedPrimitive(
                D3DPT_TRIANGLELIST,
                D3DFVF_CUSTOMVERTEX,
                bd->pVB,
                draw_data->TotalVtxCount,
                (LPWORD)idx_start,
                pcmd->ElemCount,
                0);
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
    }

    // Restore device state
    ImGui_ImplDX7_RestoreState(&old);
}

// Device object helpers

bool ImGui_ImplDX7_CreateDeviceObjects()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return false;
    // Nothing persistent to create here, everything is updated per frame
    return true;
}

void ImGui_ImplDX7_InvalidateDeviceObjects()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return;

    // Destroy all textures we own (where this backend is the last ref)
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
    {
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplDX7_UpdateTexture(tex);
        }
    }

    delete[] bd->pVB;
    delete[] bd->pIB;
    bd->pVB = nullptr;
    bd->pIB = nullptr;
    bd->VertexBufferSize = 0;
    bd->IndexBufferSize = 0;
}

// Init and shutdown

bool ImGui_ImplDX7_Init(IDirect3DDevice7* device)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Renderer backend already initialised");

    ImGui_ImplDX7_Data* bd = IM_NEW(ImGui_ImplDX7_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx7";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth  = 4096;
    platform_io.Renderer_TextureMaxHeight = 4096;

    bd->pd3dDevice = device;
    bd->pd3dDevice->AddRef();

    // Fetch parent DirectDraw interface for texture creation
    LPDIRECTDRAWSURFACE7 backbuffer = nullptr;
    if (SUCCEEDED(device->GetRenderTarget(&backbuffer)) && backbuffer)
    {
        void* dd = nullptr;
        if (SUCCEEDED(backbuffer->GetDDInterface(&dd)))
            bd->pDD = (LPDIRECTDRAW7)dd;
        backbuffer->Release();
    }

    // We always convert RGBA to ARGB, so HasRgbaSupport is left as false for now
    bd->HasRgbaSupport = false;

    return true;
}

void ImGui_ImplDX7_Shutdown()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown");

    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ImGui_ImplDX7_InvalidateDeviceObjects();

    if (bd->pDD)
        bd->pDD->Release();
    if (bd->pd3dDevice)
        bd->pd3dDevice->Release();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);

    platform_io.ClearRendererHandlers();

    IM_DELETE(bd);
}

void ImGui_ImplDX7_NewFrame()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialised. Did you call ImGui_implDX7_Init?");
    IM_UNUSED(bd);
}

#endif // #ifndef IMGUI_DISABLE
