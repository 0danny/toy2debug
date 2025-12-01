#include "Renderer.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_impl_dx9.h>

#include <MinHook.h>

#include <print>
#include <windows.h>

#include <d3d.h>
#include <ddraw.h>

namespace
{
	// Hooks
	constexpr uintptr_t g_initializeForWindowAddr = 0x004AEDA0;
}

void InitSurfaceDesc(LPDDSURFACEDESC2 p_ddSurfaceDesc, DWORD p_flags, DWORD p_caps)
{
	memset(p_ddSurfaceDesc, 0, 124u);

	p_ddSurfaceDesc->dwSize = 124;
	p_ddSurfaceDesc->dwFlags = p_flags;
	p_ddSurfaceDesc->ddsCaps.dwCaps = p_caps;
	p_ddSurfaceDesc->ddpfPixelFormat.dwSize = 32;
}

uint32_t CreateZBuffer(CD3DFramework* _this)
{
	int32_t l_width;                      // edx
	int32_t l_height;                     // eax
	LPDIRECTDRAW4 l_dd4;                  // eax
	int l_result;                         // eax
	struct _DDSURFACEDESC2 l_surfaceDesc; // [esp+8h] [ebp-7Ch] BYREF

	if ( (_this->m_ddDeviceDesc.dpcTriCaps.dwRasterCaps & 0x8000u) != 0 )
		return 0;

	InitSurfaceDesc(&l_surfaceDesc, 4103u, 0);
	l_width = _this->m_dwRenderWidth;
	l_height = _this->m_dwRenderHeight;

	l_surfaceDesc.ddsCaps.dwCaps = _this->m_dwDeviceMemType | 0x20000;
	l_surfaceDesc.dwWidth = l_width;
	l_surfaceDesc.dwHeight = l_height;
	l_dd4 = _this->m_pDD;

	memcpy(&l_surfaceDesc.ddpfPixelFormat, &_this->m_ddpfZBuffer, sizeof(l_surfaceDesc.ddpfPixelFormat));

	l_result = l_dd4->CreateSurface(&l_surfaceDesc, &_this->m_pddsZBuffer, 0);

	if ( l_result >= 0 )
		return _this->m_pddsRenderTarget->AddAttachedSurface(_this->m_pddsZBuffer) >= 0 ? 0 : 0x82000005;
	else
		return l_result != DDERR_OUTOFVIDEOMEMORY ? 0x82000005 : DDERR_OUTOFVIDEOMEMORY;
}

uint32_t CreatePrimaryChainAndRects(CD3DFramework* _this, DDAppDisplayMode* a2, uint8_t p_flags)
{
	RECT* p_m_presentDstRect;             // edi
	LONG top;                             // ecx
	LONG right;                           // eax
	LONG bottom;                          // ecx
	DWORD l_flags;                        // edi
	LONG v9;                              // eax
	LONG v10;                             // ecx
	int v12;                              // ebx
	DWORD dwFlags;                        // eax
	LPDIRECTDRAWSURFACE4* l_ddSurface4_1; // edi
	int v15;                              // eax
	LPDIRECTDRAWSURFACE4 l_surface1;      // eax
	int v17;                              // eax
	LONG v18;                             // edi
	HWND m_hWnd;                          // eax
	LONG v20;                             // edx
	int32_t v21;                          // eax
	int l_surface1Res;                    // eax
	int32_t m_width;                      // eax
	LPDIRECTDRAW4 m_dd4;                  // eax
	HWND v25;                             // [esp+20h] [ebp-A8h]
	LPDIRECTDRAWCLIPPER l_clipper;        // [esp+38h] [ebp-90h] BYREF
	DDSURFACEDESC2 l_d3dDesc;             // [esp+3Ch] [ebp-8Ch] BYREF
	DDSCAPS2 l_ddsCaps;                   // [esp+B8h] [ebp-10h] BYREF


	if ( (p_flags & 1) != 0 )
	{
		p_m_presentDstRect = &_this->m_rcViewportRect;
		SetRect(&_this->m_rcViewportRect, 0, 0, a2->surfaceDesc.dwWidth, a2->surfaceDesc.dwHeight);
		top = _this->m_rcViewportRect.top;
		_this->m_rcScreenRect.left = p_m_presentDstRect->left;
		right = _this->m_rcViewportRect.right;
		_this->m_rcScreenRect.top = top;
		bottom = _this->m_rcViewportRect.bottom;

		l_flags = 0;

		_this->m_rcScreenRect.right = right;
		v9 = _this->m_rcViewportRect.right;
		_this->m_dwRenderWidth = v9;
		_this->m_rcScreenRect.bottom = bottom;
		v10 = _this->m_rcViewportRect.bottom;
		_this->m_dwRenderHeight = v10;

		if ( v9 == 320 && v10 == 200 )
			l_flags = a2->surfaceDesc.ddpfPixelFormat.dwRGBBitCount == 8;

		if ( _this->m_pDD->SetDisplayMode(v9, v10, a2->surfaceDesc.ddpfPixelFormat.dwRGBBitCount, a2->surfaceDesc.dwMipMapCount, l_flags) < 0 )
			return 0x82000009;

		InitSurfaceDesc(&l_d3dDesc, 1u, 0);

		v12 = p_flags & 2;
		l_d3dDesc.ddsCaps.dwCaps = 8704;

		if ( (p_flags & 2) != 0 )
		{
			dwFlags = l_d3dDesc.dwFlags;
			l_d3dDesc.ddsCaps.dwCaps = 8728;
			dwFlags = l_d3dDesc.dwFlags | 0x20;
			l_d3dDesc.dwBackBufferCount = 1;
			l_d3dDesc.dwFlags = dwFlags;
		}

		l_ddSurface4_1 = &_this->m_pddsFrontBuffer;

		v15 = _this->m_pDD->CreateSurface(&l_d3dDesc, &_this->m_pddsFrontBuffer, 0);

		if ( v15 < 0 )
			return v15 != DDERR_OUTOFVIDEOMEMORY ? 0x82000007 : DDERR_OUTOFVIDEOMEMORY;

		if ( (p_flags & 2) == 0 )
			goto LABEL_28;

		l_surface1 = *l_ddSurface4_1;

		l_ddsCaps.dwCaps = 4;
		v17 = l_surface1->GetAttachedSurface(&l_ddsCaps, &_this->m_pddsBackBuffer);
	}
	else
	{
		GetClientRect((HWND)_this->m_hWnd, &_this->m_rcViewportRect);
		v18 = _this->m_rcViewportRect.bottom;
		_this->m_rcViewportRect.top += 0;
		m_hWnd = (HWND)_this->m_hWnd;
		_this->m_rcViewportRect.bottom = v18 - 0;
		GetClientRect(m_hWnd, &_this->m_rcScreenRect);

		v20 = _this->m_rcScreenRect.bottom - 0;
		_this->m_rcScreenRect.bottom = v20;
		v25 = (HWND)_this->m_hWnd;
		_this->m_rcScreenRect.bottom = v20 - 0;

		ClientToScreen(v25, (LPPOINT)&_this->m_rcScreenRect);
		ClientToScreen((HWND)_this->m_hWnd, (LPPOINT)&_this->m_rcScreenRect.right);
		v21 = _this->m_rcViewportRect.bottom;
		_this->m_dwRenderWidth = _this->m_rcViewportRect.right;
		_this->m_dwRenderHeight = v21;

		InitSurfaceDesc(&l_d3dDesc, 1u, 0);
		v12 = p_flags & 2;
		l_d3dDesc.ddsCaps.dwCaps = 512;

		if ( (p_flags & 2) == 0 )
			l_d3dDesc.ddsCaps.dwCaps = 8704;

		l_ddSurface4_1 = &_this->m_pddsFrontBuffer;
		l_surface1Res = _this->m_pDD->CreateSurface(&l_d3dDesc, &_this->m_pddsFrontBuffer, 0);

		if ( l_surface1Res < 0 )
			return l_surface1Res != DDERR_OUTOFVIDEOMEMORY ? 0x82000007 : DDERR_OUTOFVIDEOMEMORY;

		if ( _this->m_pDD->CreateClipper(0, &l_clipper, 0) < 0 )
			return -2113929208;

		l_clipper->SetHWnd(0, (HWND)_this->m_hWnd);
		(*l_ddSurface4_1)->SetClipper(l_clipper);

		if ( l_clipper )
		{
			l_clipper->Release();
			l_clipper = 0;
		}

		if ( (p_flags & 2) == 0 )
		{
			ClientToScreen((HWND)_this->m_hWnd, (LPPOINT)&_this->m_rcViewportRect);
			ClientToScreen((HWND)_this->m_hWnd, (LPPOINT)&_this->m_rcViewportRect.right);
			goto LABEL_26;
		}

		m_width = _this->m_dwRenderWidth;
		l_d3dDesc.dwHeight = _this->m_dwRenderHeight;
		l_d3dDesc.dwWidth = m_width;

		m_dd4 = _this->m_pDD;
		l_d3dDesc.dwFlags = 7;
		l_d3dDesc.ddsCaps.dwCaps = 8256;

		v17 = m_dd4->CreateSurface(&l_d3dDesc, &_this->m_pddsBackBuffer, 0);
	}
	if ( v17 < 0 )
		return v17 != DDERR_OUTOFVIDEOMEMORY ? 0x8200000A : DDERR_OUTOFVIDEOMEMORY;
LABEL_26:

	if ( ! v12 )
	{
	LABEL_28:
		_this->m_pddsRenderTarget = *l_ddSurface4_1;
		goto LABEL_29;
	}

	_this->m_pddsRenderTarget = _this->m_pddsBackBuffer;
LABEL_29:
	_this->m_pddsRenderTarget->AddRef();

	return 0;
}

HRESULT __stdcall EnumZBufferFormats(LPDDPIXELFORMAT p_lpDDPixFmt, LPVOID p_lpContext)
{
	HRESULT l_result; // eax

	l_result = 0;

	if ( p_lpDDPixFmt )
	{
		if ( p_lpContext )
		{
			if ( p_lpDDPixFmt->dwFlags != ((LPDDPIXELFORMAT)p_lpContext)->dwFlags )
				return 1;

			memcpy(p_lpContext, p_lpDDPixFmt, sizeof(DDPIXELFORMAT));

			if ( p_lpDDPixFmt->dwRGBBitCount != 16 )
				return 1;
		}
	}

	return l_result;
}

uint32_t SelectD3DDeviceAndZFormat(CD3DFramework* _this, GUID* p_deviceGuid, uint8_t p_flags)
{
	LPDIRECT3D3* l_d3d3;                 // ebp
	DWORD l_data;                        // eax
	int l_data2;                         // ecx
	LPDIRECT3D3 l_d3d3Copy;              // eax
	D3DDEVICEDESC* l_d3dDesc;            // edi
	D3DDEVICEDESC* p_ddHwDesc;           // esi
	D3DFINDDEVICESEARCH l_findDevSearch; // [esp+Ch] [ebp-268h] BYREF
	D3DFINDDEVICERESULT l_findDevResult; // [esp+68h] [ebp-20Ch] BYREF

	l_d3d3 = &_this->m_pD3D;

	std::println("SelectD3DDeviceAndZFormat! part 1");

	if ( _this->m_pDD->QueryInterface(IID_IDirect3D3, (LPVOID*)&_this->m_pD3D) < 0 )
		return 0x82000003;

	memset(&l_findDevResult, 0, sizeof(l_findDevResult));
	memset(&l_findDevSearch, 0, sizeof(l_findDevSearch));

	l_data = p_deviceGuid->Data1;

	l_findDevResult.dwSize = 524;

	l_findDevSearch.dwSize = 92;
	l_findDevSearch.dwFlags = 2;

	l_d3d3Copy = *l_d3d3;
	l_findDevSearch.guid = *p_deviceGuid;

	if ( l_d3d3Copy->FindDevice(&l_findDevSearch, &l_findDevResult) < 0 )
		return 0x82000003;

	std::println("SelectD3DDeviceAndZFormat! part 2");

	if ( l_findDevResult.ddHwDesc.dwFlags )
	{
		_this->m_dwDeviceMemType = 0x4000;
		l_d3dDesc = &_this->m_ddDeviceDesc;
		p_ddHwDesc = &l_findDevResult.ddHwDesc;
	}
	else
	{
		_this->m_dwDeviceMemType = 2048;
		l_d3dDesc = &_this->m_ddDeviceDesc;
		p_ddHwDesc = &l_findDevResult.ddSwDesc;
	}

	memcpy(l_d3dDesc, p_ddHwDesc, sizeof(D3DDEVICEDESC));
	memset(&_this->m_ddpfZBuffer, 0, sizeof(_this->m_ddpfZBuffer));

	if ( (p_flags & 8) != 0 )
		_this->m_ddpfZBuffer.dwFlags = 17408;
	else
		_this->m_ddpfZBuffer.dwFlags = 1024;

	(*l_d3d3)->EnumZBufferFormats(*p_deviceGuid, EnumZBufferFormats, &_this->m_ddpfZBuffer);

	std::println("SelectD3DDeviceAndZFormat! part 3, dwSize -> {}", _this->m_ddpfZBuffer.dwSize);

	return _this->m_ddpfZBuffer.dwSize != 32 ? 0x82000005 : 0;
}

HRESULT CreateDirectDraw(CD3DFramework* _this, GUID* p_lpGUID, uint8_t p_flags)
{
	DWORD l_coopLevel; // ecx

	LPDIRECTDRAW drawPtr;

	if ( DirectDrawCreate(p_lpGUID, &drawPtr, 0) < 0 )
		return 0x82000001;

	if ( drawPtr->QueryInterface(IID_IDirectDraw4, (LPVOID*)&_this->m_pDD) >= 0 )
	{
		drawPtr->Release();

		l_coopLevel = 8;

		if ( _this->m_bIsFullscreen )
			l_coopLevel = 19;

		if ( (p_flags & 0x10) == 0 )
			l_coopLevel |= 8u;

		return _this->m_pDD->SetCooperativeLevel(_this->m_hWnd, l_coopLevel) >= 0 ? 0 : 0x82000002;
	}
	else
	{
		drawPtr->Release();
		return 0x82000001;
	}
}

HRESULT CreateD3DDevice(CD3DFramework* _this, const CLSID* p_guid)
{
	DDSURFACEDESC2 l_surfaceDesc; // [esp+4h] [ebp-7Ch] BYREF

	l_surfaceDesc.dwSize = 124;
	_this->m_pDD->GetDisplayMode(&l_surfaceDesc);

	if ( l_surfaceDesc.ddpfPixelFormat.dwRGBBitCount > 8 )
		return _this->m_pD3D->CreateDevice(*p_guid, _this->m_pddsRenderTarget, &_this->m_pd3dDevice, 0) >= 0 ? 0 : 0x82000004;
	else
		return 0x8200000D;
}

int BuildViewport(D3DVIEWPORT2* p_viewport, DWORD p_width, DWORD p_height)
{
	int l_result; // eax

	memset(p_viewport, 0, sizeof(D3DVIEWPORT2));

	p_viewport->dwWidth = p_width;
	p_viewport->dwHeight = p_height;

	l_result = 0x40000000;

	p_viewport->dwSize = 44;
	p_viewport->dvMaxZ = 1.0;
	p_viewport->dvClipX = -1.0;
	p_viewport->dvClipWidth = 2.0;
	p_viewport->dvClipY = 1.0;
	p_viewport->dvClipHeight = 2.0;

	return l_result;
}

unsigned int CreateAndSetViewport(CD3DFramework* _this)
{
	LPDIRECT3DVIEWPORT3* l_viewport; // edi
	D3DVIEWPORT2 l_viewport2;        // [esp+14h] [ebp-2Ch] BYREF

	BuildViewport(&l_viewport2, _this->m_dwRenderWidth, _this->m_dwRenderHeight);

	l_viewport = &_this->m_pvViewport;

	if ( _this->m_pD3D->CreateViewport(&_this->m_pvViewport, 0) < 0 )
		return 0x82000006;

	if ( _this->m_pd3dDevice->AddViewport(*l_viewport) < 0 )
		return 0x82000006;

	if ( (*l_viewport)->SetViewport2(&l_viewport2) >= 0 )
		return _this->m_pd3dDevice->SetCurrentViewport(*l_viewport) >= 0 ? 0 : 0x82000006;

	return 0x82000006;
}

int32_t InitializeDeviceAndSurfaces(CD3DFramework* _this, GUID* p_ddAppGuid, GUID* p_deviceGuid, DDAppDisplayMode* p_displayMode, uint8_t p_flags)
{
	int32_t l_result = 0;

	l_result = CreateDirectDraw(_this, p_ddAppGuid, p_flags);

	if ( l_result < 0 )
		return l_result;

	std::printf("Created Direct Draw!\n");

	l_result = SelectD3DDeviceAndZFormat(_this, p_deviceGuid, p_flags);

	if ( ! p_deviceGuid || l_result < 0 )
		return l_result;

	std::printf("Selected D3D Device & Z Format!, This PTR -> %p\n", _this);

	l_result = CreatePrimaryChainAndRects(_this, p_displayMode, p_flags);

	std::printf("CreatePrimaryChainAndRects -> %d, This PTR -> %p\n", l_result, _this);

	if ( l_result < 0 )
		return l_result;

	std::printf("Device GUID -> %p, This PTR -> %p\n", p_deviceGuid, _this);

	if ( ! p_deviceGuid )
		return 0;

	l_result = CreateZBuffer(_this);

	std::printf("ZBuffer -> %d !\n", l_result);

	if ( l_result < 0 ) // (p_flags & 4) != 0 ||
		return l_result;

	l_result = CreateD3DDevice(_this, p_deviceGuid);

	std::printf("Created D3D Device!\n");

	if ( l_result < 0 )
		return l_result;

	l_result = CreateAndSetViewport(_this);

	if ( l_result < 0 )
		return l_result;

	if ( _this->m_initialized )
		return 0x82000000;

	_this->m_slots[0].m_width = _this->m_dwRenderWidth;
	_this->m_slots[0].m_height = _this->m_dwRenderHeight;
	_this->m_slots[0].m_surface1 = _this->m_pddsBackBuffer;
	_this->m_slots[0].m_valid = 1;
	_this->m_slots[0].m_surface2 = _this->m_pddsZBuffer;

	_this->m_initialized = 1;

	return 0;
}

int32_t __fastcall Hook_InitalizeForWindow(
    CD3DFramework* _this,
    void* EDX,
    HWND p_hWnd,
    GUID* p_ddAppGuid,
    DDAppDevice* p_device,
    DDAppDisplayMode* p_displayMode,
    uint8_t p_flags
)
{
	std::println("InitializeForWindow!");

	if ( ! p_hWnd || ! p_displayMode && (p_flags & 1) != 0 )
		return DDERR_INVALIDPARAMS;

	_this->m_hWnd = p_hWnd;
	_this->m_bIsFullscreen = p_flags & 1;

	return InitializeDeviceAndSurfaces(_this, p_ddAppGuid, &p_device->guid, p_displayMode, p_flags);
}

void Renderer::Init()
{
	if ( MH_Initialize() )
	{
		std::println("[Hooks]: Could not init MinHook.");
		return;
	}

	MH_CreateHook(reinterpret_cast<LPVOID>(g_initializeForWindowAddr), reinterpret_cast<LPVOID>(&Hook_InitalizeForWindow), nullptr);

	std::println("Hook has been created.");

	if ( MH_EnableHook(MH_ALL_HOOKS) )
		std::println("[Hooks]: Could not enable all hooks.");
}

void Renderer::Render() {}
