#include "Renderer.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_impl_dx9.h>
#include <MinHook.h>

#include <print>
#include <windows.h>

#include <d3d9.h>
#include <d3dtypes.h>

namespace
{
	constexpr uintptr_t g_initializeForWindowAddr = 0x004AEDA0;
	constexpr uintptr_t g_createMaterialAddr = 0x004AC100;
	constexpr uintptr_t g_setMaterialAddr = 0x004AC140;
	constexpr uintptr_t g_getHandleAddr = 0x004AC120;

	constexpr uintptr_t g_createLightAddr = 0x004ABF30;
	constexpr uintptr_t g_setLightAddr = 0x004ABF50;
	constexpr uintptr_t g_addLightAddr = 0x004ABF60;
	constexpr uintptr_t g_setLightStateAddr = 0x004ABFA0;
	constexpr uintptr_t g_deleteLightAddr = 0x004ABF80;
	constexpr uintptr_t g_releaseLightAddr = 0x004ABFC0;

	constexpr uintptr_t g_intialiseTextureSurfaceAddr = 0x004B0200;
	constexpr uintptr_t g_glueSetBackdropAddr = 0x004CE380;
	constexpr uintptr_t g_clearScreenAddr = 0x004ABAF0;

	constexpr uintptr_t g_beginSceneAddr = 0x004ABA90;
	constexpr uintptr_t g_endSceneAddr = 0x004ABAD0;
	constexpr uintptr_t g_presentFrameAddr = 0x004ABD40;
	constexpr uintptr_t g_setViewportAddr = 0x004ABB30;

	constexpr uintptr_t g_releaseVertexBufferAddr = 0x004AC070;
	constexpr uintptr_t g_createVertexBufferAddr = 0x004AC080;
	constexpr uintptr_t g_lockVertexBufferAddr = 0x004AC0A0;
	constexpr uintptr_t g_unlockVertexBufferAddr = 0x004AC0C0;
	constexpr uintptr_t g_optimizeVertexBufferAddr = 0x004AC0D0;
	constexpr uintptr_t g_processVertsOnBufferAddr = 0x004AC030;

	constexpr uintptr_t g_drawIndexedPrimVBAddr = 0x004AC340;
	constexpr uintptr_t g_drawIndexedPrimAddr = 0x004AC300;

	constexpr uintptr_t g_setViewTransformAddr = 0x004ABFF0;
	constexpr uintptr_t g_setProjTransformAddr = 0x004AC010;
	constexpr uintptr_t g_setWorldTransformAddr = 0x004ABFD0;
	constexpr uintptr_t g_setTextureAddr = 0x004AC1A0;

	constexpr uintptr_t g_setRenderState = 0x004AC370;
	constexpr uintptr_t g_setTextureStageAddr = 0x004AC150;
	constexpr uintptr_t g_backdropBltFastAddr = 0x004CE4D0;

	// Software Renderer
	constexpr uintptr_t g_softwareRenderer1Addr = 0x004BCE00;
	constexpr uintptr_t g_fontBuilderAddr = 0x004B4110;

	static CD3DFramework* frameworkVar = nullptr;

}

struct NuLight9
{
	D3DLIGHT9 light;
	INT index; // -1 means not assigned yet
};

static UINT g_nextLightIndex = 0;

namespace Glue
{
	static int32_t g_selectedTex = 0;
	static IDirect3DSurface9* g_backdrop = 0;
}

uint32_t CreateZBuffer(CD3DFramework* _this)
{
	return 0;
}

uint32_t CreatePrimaryChainAndRects(CD3DFramework* _this, DDAppDisplayMode* a2, uint8_t p_flags)
{
	if ( (p_flags & 1) != 0 )
	{
		// Fullscreen mode
		SetRect(&_this->m_rcViewportRect, 0, 0, a2->surfaceDesc.dwWidth, a2->surfaceDesc.dwHeight);
		_this->m_rcScreenRect = _this->m_rcViewportRect;
		_this->m_dwRenderWidth = a2->surfaceDesc.dwWidth;
		_this->m_dwRenderHeight = a2->surfaceDesc.dwHeight;
	}
	else
	{
		// Windowed mode
		GetClientRect((HWND)_this->m_hWnd, &_this->m_rcViewportRect);
		GetClientRect((HWND)_this->m_hWnd, &_this->m_rcScreenRect);

		ClientToScreen((HWND)_this->m_hWnd, (LPPOINT)&_this->m_rcScreenRect);
		ClientToScreen((HWND)_this->m_hWnd, (LPPOINT)&_this->m_rcScreenRect.right);

		_this->m_dwRenderWidth = _this->m_rcViewportRect.right;
		_this->m_dwRenderHeight = _this->m_rcViewportRect.bottom;
	}

	return 0;
}

HRESULT WINAPI EnumZBufferFormats(LPDDPIXELFORMAT p_lpDDPixFmt, LPVOID p_lpContext)
{
	// This callback is no longer needed in DX9 but kept for compatibility
	if ( p_lpDDPixFmt && p_lpContext )
	{
		if ( p_lpDDPixFmt->dwFlags != ((LPDDPIXELFORMAT)p_lpContext)->dwFlags )
			return 1;
		memcpy(p_lpContext, p_lpDDPixFmt, sizeof(DDPIXELFORMAT));
		if ( p_lpDDPixFmt->dwRGBBitCount != 16 )
			return 1;
	}
	return 0;
}

uint32_t SelectD3DDeviceAndZFormat(CD3DFramework* _this, GUID* p_deviceGuid, uint8_t p_flags)
{
	std::println("SelectD3DDeviceAndZFormat! part 1");

	// Just set some defaults for compatibility
	_this->m_dwDeviceMemType = 0x4000; // Hardware

	// Set up Z-buffer format info for compatibility
	memset(&_this->m_ddpfZBuffer, 0, sizeof(_this->m_ddpfZBuffer));
	_this->m_ddpfZBuffer.dwSize = 32;
	_this->m_ddpfZBuffer.dwFlags = (p_flags & 8) != 0 ? 17408 : 1024;
	_this->m_ddpfZBuffer.dwRGBBitCount = 16;

	_this->m_ddDeviceDesc.dpcTriCaps.dwShadeCaps = 0x4000 // alpha blending supported
	    | 0x8000                                          // alpha gouraud
	    | 0x40                                            // color gouraud
	    | 0x20;                                           // color flat

	_this->m_ddDeviceDesc.dpcTriCaps.dwDestBlendCaps = 0x0002 // D3DBLEND_ONE
	    | 0x0008                                              // D3DBLEND_INVSRCALPHA
	    | 0x0040;                                             // SRCALPHA

	_this->m_ddDeviceDesc.dpcTriCaps.dwSrcBlendCaps = 0x0001 | 0x0002 | 0x0020 | 0x0040;

	_this->m_ddDeviceDesc.dwTextureOpCaps = 0x1 | 0x4;
	_this->m_ddDeviceDesc.dpcTriCaps.dwTextureFilterCaps = 0x10;

	_this->m_ddDeviceDesc.wMaxSimultaneousTextures = 1;

	std::println("SelectD3DDeviceAndZFormat! part 2");
	std::println("SelectD3DDeviceAndZFormat! part 3, dwSize -> {}", _this->m_ddpfZBuffer.dwSize);

	return 0;
}

HRESULT CreateDirectDraw(CD3DFramework* _this, GUID* p_lpGUID, uint8_t p_flags)
{
	// In DX9, Direct3DCreate9 replaces DirectDraw
	_this->m_pDD = Direct3DCreate9(D3D_SDK_VERSION);

	if ( ! _this->m_pDD )
		return 0x82000001;

	return 0;
}

HRESULT CreateD3DDevice(CD3DFramework* _this, const CLSID* p_guid)
{
	IDirect3D9* pD3D = (IDirect3D9*)_this->m_pDD;

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));

	d3dpp.Windowed = ! _this->m_bIsFullscreen;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferWidth = _this->m_dwRenderWidth;
	d3dpp.BackBufferHeight = _this->m_dwRenderHeight;
	d3dpp.BackBufferFormat = _this->m_bIsFullscreen ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.hDeviceWindow = (HWND)_this->m_hWnd;

	HRESULT hr = pD3D->CreateDevice(
	    D3DADAPTER_DEFAULT,
	    D3DDEVTYPE_HAL,
	    (HWND)_this->m_hWnd,
	    D3DCREATE_HARDWARE_VERTEXPROCESSING,
	    &d3dpp,
	    (IDirect3DDevice9**)&_this->m_pd3dDevice
	);

	if ( FAILED(hr) )
	{
		// Try software vertex processing
		hr = pD3D->CreateDevice(
		    D3DADAPTER_DEFAULT,
		    D3DDEVTYPE_HAL,
		    (HWND)_this->m_hWnd,
		    D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		    &d3dpp,
		    (IDirect3DDevice9**)&_this->m_pd3dDevice
		);
	}

	if ( FAILED(hr) )
		return 0x82000004;

	// Get back buffer reference
	IDirect3DDevice9* pDevice = (IDirect3DDevice9*)_this->m_pd3dDevice;
	pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, (IDirect3DSurface9**)&_this->m_pddsBackBuffer);

	// Get depth stencil surface
	pDevice->GetDepthStencilSurface((IDirect3DSurface9**)&_this->m_pddsZBuffer);

	// Set render target
	_this->m_pddsRenderTarget = _this->m_pddsBackBuffer;
	if ( _this->m_pddsRenderTarget )
		((IDirect3DSurface9*)_this->m_pddsRenderTarget)->AddRef();

	return 0;
}

int32_t CreateAndSetViewport(CD3DFramework* _this)
{
	// Set DX9 viewport
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width = _this->m_dwRenderWidth;
	vp.Height = _this->m_dwRenderHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;

	IDirect3DDevice9* pDevice = (IDirect3DDevice9*)_this->m_pd3dDevice;
	if ( FAILED(pDevice->SetViewport(&vp)) )
		return 0x82000006;

	// Store a dummy pointer for compatibility (the game may check this)
	_this->m_pvViewport = (void*)1; // Non-null dummy value

	return 0;
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

	if ( l_result < 0 )
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

	frameworkVar = _this;

	if ( ! p_hWnd || (! p_displayMode && (p_flags & 1) != 0) )
		return DDERR_INVALIDPARAMS;

	p_flags &= ~0x1; // Clear bit 0 to disable fullscreen

	_this->m_hWnd = p_hWnd;
	_this->m_bIsFullscreen = p_flags & 1;

	return InitializeDeviceAndSurfaces(_this, p_ddAppGuid, &p_device->guid, p_displayMode, p_flags);
}

HRESULT __cdecl Hook_CreateMaterial(D3DMATERIAL9** p_outMaterial)
{
	std::printf("Creating new material.\n");

	D3DMATERIAL9* pMaterial = new D3DMATERIAL9;
	ZeroMemory(pMaterial, sizeof(D3DMATERIAL9));

	*p_outMaterial = pMaterial;

	return D3D_OK;
}

HRESULT __cdecl Hook_SetMaterial(D3DMATERIAL9* p_d3dMaterial9, LPD3DMATERIAL p_d3dMaterial)
{
	std::printf("Material %p\n", p_d3dMaterial);

	p_d3dMaterial9->Diffuse = p_d3dMaterial->diffuse;
	p_d3dMaterial9->Ambient = p_d3dMaterial->ambient;
	p_d3dMaterial9->Specular = p_d3dMaterial->specular;
	p_d3dMaterial9->Emissive = p_d3dMaterial->emissive;
	p_d3dMaterial9->Power = p_d3dMaterial->power;

	return D3D_OK;
}

HRESULT __cdecl Hook_GetHandle(D3DMATERIAL9* p_d3dMaterial9, void** p_d3dMaterialHandle)
{
	*p_d3dMaterialHandle = p_d3dMaterial9;

	return D3D_OK;
}

HRESULT __cdecl Hook_SetLightState(D3DLIGHTSTATETYPE p_lightState, DWORD p_value)
{
	if ( ! frameworkVar || ! frameworkVar->m_pd3dDevice )
		return E_FAIL;

	IDirect3DDevice9* device = frameworkVar->m_pd3dDevice;

	switch ( p_lightState )
	{
		case D3DLIGHTSTATE_AMBIENT:
			// Global ambient colour
			return device->SetRenderState(D3DRS_AMBIENT, p_value);

		case D3DLIGHTSTATE_COLORVERTEX:
			// Use vertex colours in lighting
			return device->SetRenderState(D3DRS_COLORVERTEX, p_value ? TRUE : FALSE);

		// TODO: Add other mappings if needed

		default:
			// Unknown or unused state - ignore safely
			return D3D_OK;
	}
}


HRESULT __cdecl Hook_CreateLight(void** p_d3dLight)
{
	if ( ! p_d3dLight )
		return E_INVALIDARG;

	NuLight9* light = new NuLight9;
	ZeroMemory(&light->light, sizeof(D3DLIGHT9));

	// Reasonable defaults
	light->light.Type = D3DLIGHT_DIRECTIONAL;
	light->light.Range = 1000.0f;
	light->index = -1;

	*p_d3dLight = light;
	return D3D_OK;
}

HRESULT __cdecl Hook_SetLight(void* p_light, void* p_d3dLight)
{
	if ( ! p_light || ! p_d3dLight )
		return E_INVALIDARG;

	NuLight9* dst = reinterpret_cast<NuLight9*>(p_light);
	D3DLIGHT* src = reinterpret_cast<D3DLIGHT*>(p_d3dLight);

	ZeroMemory(&dst->light, sizeof(D3DLIGHT9));

	dst->light.Type = src->dltType;
	// Old struct only has one colour, so use it for all three
	dst->light.Diffuse = src->dcvColor;
	dst->light.Specular = src->dcvColor;
	dst->light.Ambient = src->dcvColor;

	dst->light.Position = src->dvPosition;
	dst->light.Direction = src->dvDirection;
	dst->light.Range = src->dvRange;
	dst->light.Falloff = src->dvFalloff;
	dst->light.Attenuation0 = src->dvAttenuation0;
	dst->light.Attenuation1 = src->dvAttenuation1;
	dst->light.Attenuation2 = src->dvAttenuation2;
	dst->light.Theta = src->dvTheta;
	dst->light.Phi = src->dvPhi;

	return D3D_OK;
}


HRESULT __cdecl Hook_AddLight(void* p_light)
{
	if ( ! p_light || ! frameworkVar || ! frameworkVar->m_pd3dDevice )
		return E_FAIL;

	IDirect3DDevice9* device = frameworkVar->m_pd3dDevice;
	NuLight9* l = reinterpret_cast<NuLight9*>(p_light);

	// Assign an index if we do not have one yet
	if ( l->index < 0 )
	{
		D3DCAPS9 caps;
		UINT maxLights = 8;

		if ( SUCCEEDED(device->GetDeviceCaps(&caps)) )
			maxLights = caps.MaxActiveLights;

		// Very simple allocator - just increment
		l->index = static_cast<INT>(g_nextLightIndex++ % maxLights);
	}

	HRESULT hr = device->SetLight(l->index, &l->light);
	if ( FAILED(hr) )
		return hr;

	hr = device->LightEnable(l->index, TRUE);
	return hr;
}

HRESULT __cdecl Hook_DeleteLight(void* p_light)
{
	// TODO: Fix
	return 0;
}

HRESULT __cdecl Hook_ReleaseLight(void* p_light)
{
	// TODO: Fix
	return 0;
}

void Nu3D_DestroyBmpDataNode(Nu3DBmpDataNode* p_bmpDataNode)
{
	auto l_d3dTexture = p_bmpDataNode->d3dTexture;

	if ( l_d3dTexture )
	{
		l_d3dTexture->Release();
		p_bmpDataNode->d3dTexture = 0;
	}
}

void __cdecl Nu3D_CalculatePixelFormatShifts(Nu3DPixelFormatInfo* p_pixelFormatInfo, D3DFORMAT format)
{
	// Set up masks and shifts based on the D3DFORMAT
	switch ( format )
	{
		case D3DFMT_A8R8G8B8:
			p_pixelFormatInfo->alphaMask = 0xFF000000;
			p_pixelFormatInfo->redMask = 0x00FF0000;
			p_pixelFormatInfo->greenMask = 0x0000FF00;
			p_pixelFormatInfo->blueMask = 0x000000FF;
			p_pixelFormatInfo->alphaShift = 24;
			p_pixelFormatInfo->redShift = 16;
			p_pixelFormatInfo->greenShift = 8;
			p_pixelFormatInfo->blueShift = 0;
			break;

		case D3DFMT_X8R8G8B8:
			p_pixelFormatInfo->alphaMask = 0;
			p_pixelFormatInfo->redMask = 0x00FF0000;
			p_pixelFormatInfo->greenMask = 0x0000FF00;
			p_pixelFormatInfo->blueMask = 0x000000FF;
			p_pixelFormatInfo->alphaShift = 0;
			p_pixelFormatInfo->redShift = 16;
			p_pixelFormatInfo->greenShift = 8;
			p_pixelFormatInfo->blueShift = 0;
			break;

		case D3DFMT_A4R4G4B4:
			p_pixelFormatInfo->alphaMask = 0xF000;
			p_pixelFormatInfo->redMask = 0x0F00;
			p_pixelFormatInfo->greenMask = 0x00F0;
			p_pixelFormatInfo->blueMask = 0x000F;
			p_pixelFormatInfo->alphaShift = 12 - 24; // Shift from 8-bit to 4-bit
			p_pixelFormatInfo->redShift = 8 - 16;
			p_pixelFormatInfo->greenShift = 4 - 8;
			p_pixelFormatInfo->blueShift = 0 - 0;
			break;

		case D3DFMT_A1R5G5B5:
			p_pixelFormatInfo->alphaMask = 0x8000;
			p_pixelFormatInfo->redMask = 0x7C00;
			p_pixelFormatInfo->greenMask = 0x03E0;
			p_pixelFormatInfo->blueMask = 0x001F;
			p_pixelFormatInfo->alphaShift = 15 - 24; // Shift from 8-bit to 1-bit
			p_pixelFormatInfo->redShift = 10 - 16;
			p_pixelFormatInfo->greenShift = 5 - 8;
			p_pixelFormatInfo->blueShift = 0 - 0;
			break;

		case D3DFMT_R5G6B5:
			p_pixelFormatInfo->alphaMask = 0;
			p_pixelFormatInfo->redMask = 0xF800;
			p_pixelFormatInfo->greenMask = 0x07E0;
			p_pixelFormatInfo->blueMask = 0x001F;
			p_pixelFormatInfo->alphaShift = 0;
			p_pixelFormatInfo->redShift = 11 - 16;
			p_pixelFormatInfo->greenShift = 5 - 8;
			p_pixelFormatInfo->blueShift = 0 - 0;
			break;

		default:
			// Default to 32-bit
			p_pixelFormatInfo->alphaMask = 0xFF000000;
			p_pixelFormatInfo->redMask = 0x00FF0000;
			p_pixelFormatInfo->greenMask = 0x0000FF00;
			p_pixelFormatInfo->blueMask = 0x000000FF;
			p_pixelFormatInfo->alphaShift = 24;
			p_pixelFormatInfo->redShift = 16;
			p_pixelFormatInfo->greenShift = 8;
			p_pixelFormatInfo->blueShift = 0;
			break;
	}
}

void __cdecl Nu3D_CopyTextureToSurface(Nu3DBmpDataNode* p_bitmapDataNode)
{
	LPDIRECT3DTEXTURE9 pTexture = p_bitmapDataNode->d3dTexture;
	if ( ! pTexture || ! p_bitmapDataNode->texData )
		return;

	D3DSURFACE_DESC surfaceDesc{};
	if ( FAILED(pTexture->GetLevelDesc(0, &surfaceDesc)) )
		return;

	D3DLOCKED_RECT lockedRect{};
	if ( FAILED(pTexture->LockRect(0, &lockedRect, nullptr, 0)) )
		return;

	const int texWidth = p_bitmapDataNode->textureWidth;
	const int texHeight = p_bitmapDataNode->textureHeight;

	const D3DFORMAT fmt = surfaceDesc.Format;

	for ( int y = 0; y < texHeight; ++y )
	{
		// D3D surfaces are top down, texData is bottom up in the original code
		int srcY = texHeight - 1 - y;

		uint8_t* dstRow = (uint8_t*)lockedRect.pBits + y * lockedRect.Pitch;

		for ( int x = 0; x < texWidth; ++x )
		{
			uint32_t argb = p_bitmapDataNode->texData[srcY * texWidth + x];

			// If this comes out with swapped channels, try treating it as BGRA instead.
			uint8_t a = (uint8_t)((argb >> 24) & 0xFF);
			uint8_t r = (uint8_t)((argb >> 16) & 0xFF);
			uint8_t g = (uint8_t)((argb >> 8) & 0xFF);
			uint8_t b = (uint8_t)(argb & 0xFF);

			if ( fmt == D3DFMT_A8R8G8B8 || fmt == D3DFMT_X8R8G8B8 )
			{
				uint32_t* dst32 = (uint32_t*)dstRow;
				uint32_t pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | ((uint32_t)b);

				dst32[x] = pixel;
			}
			else if ( fmt == D3DFMT_A1R5G5B5 )
			{
				uint16_t* dst16 = (uint16_t*)dstRow;

				uint16_t aa = (a >= 128) ? 1u : 0u;
				uint16_t rr = (uint16_t)(r >> 3);
				uint16_t gg = (uint16_t)(g >> 3);
				uint16_t bb = (uint16_t)(b >> 3);

				dst16[x] = (aa << 15) | (rr << 10) | (gg << 5) | bb;
			}
			else if ( fmt == D3DFMT_A4R4G4B4 )
			{
				uint16_t* dst16 = (uint16_t*)dstRow;

				uint16_t aa = (uint16_t)(a >> 4);
				uint16_t rr = (uint16_t)(r >> 4);
				uint16_t gg = (uint16_t)(g >> 4);
				uint16_t bb = (uint16_t)(b >> 4);

				dst16[x] = (aa << 12) | (rr << 8) | (gg << 4) | bb;
			}
			else
			{
				// Unknown format, skip. You can log here if needed.
			}
		}
	}

	pTexture->UnlockRect(0);
}


int32_t __cdecl Hook_InitialiseTextureSurface(Nu3DBmpDataNode* p_bmpDataNode)
{
	uint32_t l_width;
	uint32_t l_height;
	int32_t l_flags;
	uint32_t* l_texData;

	if ( (p_bmpDataNode->flags & 0x40) != 0 )
		return 0;

	Nu3D_DestroyBmpDataNode(p_bmpDataNode);

	// Get device caps (DX9 style)
	D3DCAPS9 l_deviceCaps;
	if ( FAILED(frameworkVar->m_pd3dDevice->GetDeviceCaps(&l_deviceCaps)) )
		return 0;

	l_width = p_bmpDataNode->textureWidth;
	l_height = p_bmpDataNode->textureHeight;
	l_flags = p_bmpDataNode->flags;

	// Determine texture format based on alpha requirements
	D3DFORMAT textureFormat = D3DFMT_X8R8G8B8; // Default: no alpha

	if ( (l_flags & 0xF) != 0 )
	{
		// Need alpha channel
		int minAlphaBits = (l_flags & 1) != 0 ? 4 : 1;

		if ( minAlphaBits >= 4 )
			textureFormat = D3DFMT_A4R4G4B4; // 4-bit alpha
		else
			textureFormat = D3DFMT_A1R5G5B5; // 1-bit alpha
	}

	// Create texture (replaces CreateSurface + QueryInterface)
	IDirect3DTexture9* pTexture = nullptr;
	HRESULT hr = frameworkVar->m_pd3dDevice->CreateTexture(
	    l_width,
	    l_height,
	    1, // mip levels (1 = no mipmaps)
	    0, // usage
	    textureFormat,
	    D3DPOOL_MANAGED, // managed pool for easy access
	    &pTexture,
	    nullptr
	);

	if ( FAILED(hr) || ! pTexture )
		return 0;

	// Store texture pointer (replaces surface and d3dTexture)
	p_bmpDataNode->d3dTexture = pTexture;
	p_bmpDataNode->surface = nullptr; // Not used in DX9

	D3DSURFACE_DESC surfaceDesc;
	if ( SUCCEEDED(pTexture->GetLevelDesc(0, &surfaceDesc)) )
	{
		p_bmpDataNode->surfaceDesc.dwWidth = surfaceDesc.Width;
		p_bmpDataNode->surfaceDesc.dwHeight = surfaceDesc.Height;
	}

	l_texData = p_bmpDataNode->texData;

	if ( l_texData )
		Nu3D_CopyTextureToSurface(p_bmpDataNode);

	return 1;
}

void GlueReleaseBackdrop() { Glue::g_selectedTex = -1; }

void Nu3D_GetDIBPixelColor(BGRA* p_color, DIBSECTION* p_dibSection, RGBQUAD* p_colorTables, int32_t* p_rowBasePtr, int32_t p_xOffset)
{
	p_color->a = 0xFF; // Default alpha to opaque

	int32_t bitDepth = p_dibSection->dsBmih.biBitCount;

	if ( bitDepth == 8 )
	{
		// 8-bit indexed color
		uint8_t* pixelPtr = (uint8_t*)p_rowBasePtr;
		uint8_t index = pixelPtr[p_xOffset];
		p_color->b = p_colorTables[index].rgbBlue;
		p_color->g = p_colorTables[index].rgbGreen;
		p_color->r = p_colorTables[index].rgbRed;
	}
	else if ( bitDepth == 24 )
	{
		// 24-bit RGB
		uint8_t* pixelPtr = (uint8_t*)p_rowBasePtr + (p_xOffset * 3);
		p_color->b = pixelPtr[0];
		p_color->g = pixelPtr[1];
		p_color->r = pixelPtr[2];
	}
	else if ( bitDepth == 32 )
	{
		// 32-bit BGRA
		uint8_t* pixelPtr = (uint8_t*)p_rowBasePtr + (p_xOffset * 4);
		p_color->b = pixelPtr[0];
		p_color->g = pixelPtr[1];
		p_color->r = pixelPtr[2];
		p_color->a = pixelPtr[3];
	}
	else if ( bitDepth == 16 )
	{
		// 16-bit RGB (5-6-5 or 5-5-5)
		uint16_t* pixelPtr = (uint16_t*)p_rowBasePtr;
		uint16_t pixel = pixelPtr[p_xOffset];

		// Assume 5-6-5 format
		p_color->b = ((pixel & 0x001F) << 3);
		p_color->g = ((pixel & 0x07E0) >> 3);
		p_color->r = ((pixel & 0xF800) >> 8);
	}
}

bool Nu3D_CopyToDDSurface(int32_t texIndex, IDirect3DSurface9* p_d3dSurface)
{
	static NGNTextureData* texDataFreeList = (NGNTextureData*)0x009F6224;

	if ( ! texIndex || ! p_d3dSurface )
		return false;

	auto* p_bmpDataNode = texDataFreeList[texIndex].bmpDataNode;
	if ( ! p_bmpDataNode )
		return false;

	HBITMAP l_bmpHandle = p_bmpDataNode->bitmapHandle;
	if ( ! l_bmpHandle )
		return false;

	DIBSECTION l_dibSection{};
	if ( ! GetObjectA(l_bmpHandle, sizeof(DIBSECTION), &l_dibSection) )
		return false;

	// Surface info
	D3DSURFACE_DESC surfaceDesc{};
	if ( FAILED(p_d3dSurface->GetDesc(&surfaceDesc)) )
		return false;

	const D3DFORMAT fmt = surfaceDesc.Format;

	// Lock
	D3DLOCKED_RECT lockedRect{};
	if ( FAILED(p_d3dSurface->LockRect(&lockedRect, nullptr, 0)) )
		return false;

	// DC for palette reads
	HDC l_dc = CreateCompatibleDC(nullptr);
	if ( ! l_dc )
	{
		p_d3dSurface->UnlockRect();
		return false;
	}

	HGDIOBJ l_oldObject = SelectObject(l_dc, p_bmpDataNode->bitmapHandle);

	RGBQUAD l_colorTables[256]{};
	if ( l_dibSection.dsBmih.biBitCount == 8 )
		GetDIBColorTable(l_dc, 0, 256, l_colorTables);

	const DWORD dstWidth = surfaceDesc.Width;
	const DWORD dstHeight = surfaceDesc.Height;

	bool ok = true;

	for ( DWORD y = 0; y < dstHeight; ++y )
	{
		// Source row (bottom up)
		int srcRow = l_dibSection.dsBm.bmHeight - 1 - (int)(y * l_dibSection.dsBm.bmHeight / dstHeight);

		int32_t* srcRowPtr = (int32_t*)((uint8_t*)l_dibSection.dsBm.bmBits + l_dibSection.dsBm.bmWidthBytes * srcRow);

		uint8_t* dstRowPtr = (uint8_t*)lockedRect.pBits + y * lockedRect.Pitch;

		for ( DWORD x = 0; x < dstWidth; ++x )
		{
			// Sample from source image with scaling
			int srcX = (int)(x * l_dibSection.dsBm.bmWidth / dstWidth);

			BGRA c{};
			Nu3D_GetDIBPixelColor(&c, &l_dibSection, l_colorTables, srcRowPtr, srcX);

			if ( fmt == D3DFMT_X8R8G8B8 || fmt == D3DFMT_A8R8G8B8 )
			{
				// Pack to 0xAARRGGBB (alpha full)
				uint32_t* dst32 = (uint32_t*)dstRowPtr;
				uint32_t pixel = (0xFFu << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | ((uint32_t)c.b);

				dst32[x] = pixel;
			}
			else if ( fmt == D3DFMT_R5G6B5 )
			{
				uint16_t* dst16 = (uint16_t*)dstRowPtr;

				uint16_t r = (uint16_t)(c.r >> 3); // 5 bits
				uint16_t g = (uint16_t)(c.g >> 2); // 6 bits
				uint16_t b = (uint16_t)(c.b >> 3); // 5 bits

				dst16[x] = (r << 11) | (g << 5) | b;
			}
			else if ( fmt == D3DFMT_A1R5G5B5 )
			{
				uint16_t* dst16 = (uint16_t*)dstRowPtr;

				uint16_t a = (c.a >= 128) ? 1u : 0u;
				uint16_t r = (uint16_t)(c.r >> 3);
				uint16_t g = (uint16_t)(c.g >> 3);
				uint16_t b = (uint16_t)(c.b >> 3);

				dst16[x] = (a << 15) | (r << 10) | (g << 5) | b;
			}
			else
			{
				// Unknown format, bail
				ok = false;
				break;
			}
		}

		if ( ! ok )
			break;
	}

	SelectObject(l_dc, l_oldObject);
	DeleteDC(l_dc);

	p_d3dSurface->UnlockRect();

	return ok;
}


int32_t __cdecl Hook_GlueSetBackdrop(int32_t p_textureIndex)
{
	typedef int32_t(__cdecl * GetTextureDataIndex)(int32_t p_textureIndex);
	GetTextureDataIndex getTextureDataIndex = (GetTextureDataIndex)0x004CE2C0;

	int32_t l_texIndex = getTextureDataIndex(p_textureIndex);

	if ( l_texIndex == Glue::g_selectedTex )
		return 1;

	GlueReleaseBackdrop();
	;
	Glue::g_selectedTex = l_texIndex;

	typedef HBITMAP(__cdecl * GetBmpHandle)(int32_t p_textureIndex);
	GetBmpHandle getBmpHandle = (GetBmpHandle)0x004BB6C0;

	HBITMAP l_bmpHandle = getBmpHandle(l_texIndex);
	if ( ! l_bmpHandle )
		return 0;

	auto* pDevice = frameworkVar->m_pd3dDevice;
	if ( ! pDevice )
		return 0;

	// Get back buffer surface description to match dimensions
	IDirect3DSurface9* pBackBuffer = nullptr;
	pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

	if ( ! pBackBuffer )
		return 0;

	D3DSURFACE_DESC backBufferDesc;
	pBackBuffer->GetDesc(&backBufferDesc);
	pBackBuffer->Release();

	std::println("Glue::SetBackdrop");

	// Create system memory texture (replaces DDSCAPS_SYSTEMMEMORY surface)
	IDirect3DTexture9* pTexture = nullptr;
	HRESULT hr = pDevice->CreateTexture(
	    backBufferDesc.Width,
	    backBufferDesc.Height,
	    1,                     // 1 mip level
	    0,                     // default usage
	    backBufferDesc.Format, // match back buffer format
	    D3DPOOL_SYSTEMMEM,     // system memory (replaces DDSCAPS_SYSTEMMEMORY)
	    &pTexture,
	    nullptr
	);

	if ( FAILED(hr) )
	{
		std::println("Failed to create system memory texture");

		// Try with managed pool as fallback
		hr = pDevice->CreateTexture(
		    backBufferDesc.Width,
		    backBufferDesc.Height,
		    1,
		    0,
		    backBufferDesc.Format,
		    D3DPOOL_MANAGED, // managed pool fallback
		    &pTexture,
		    nullptr
		);

		if ( FAILED(hr) )
		{
			std::println("Failed to create managed texture");
			return 0;
		}
	}

	IDirect3DSurface9* pSurface = nullptr;
	pTexture->GetSurfaceLevel(0, &pSurface);

	// Copy bitmap data to the texture surface
	if ( ! Nu3D_CopyToDDSurface(l_texIndex, pSurface) )
	{
		pTexture->Release();
		return 0;
	}

	pTexture->Release(); // Surface keeps its own reference

	Glue::g_backdrop = pSurface;

	return 1;
}

HRESULT __cdecl Hook_ClearScreen(DWORD p_clearFlags, D3DCOLOR p_clearColor)
{
	IDirect3DDevice9* pDevice = frameworkVar->m_pd3dDevice;
	if ( ! pDevice )
		return -1;

	// Convert D3D3 clear flags to DX9 flags
	DWORD dx9ClearFlags = 0;

	// D3DCLEAR_TARGET = 1 in both D3D3 and DX9
	if ( p_clearFlags & D3DCLEAR_TARGET )
		dx9ClearFlags |= D3DCLEAR_TARGET;

	// D3DCLEAR_ZBUFFER = 2 in both D3D3 and DX9
	if ( p_clearFlags & D3DCLEAR_ZBUFFER )
		dx9ClearFlags |= D3DCLEAR_ZBUFFER;

	// In DX9, Clear() clears the entire viewport by default
	// If you need to clear a specific rectangle, you can pass it as the second parameter

	typedef RECT*(__cdecl * GetDestRect)();
	GetDestRect getDestRect = (GetDestRect)0x004ABD80;

	RECT* l_destRect = getDestRect();

	if ( l_destRect )
	{
		// Clear specific rectangle
		D3DRECT d3dRect;
		d3dRect.x1 = l_destRect->left;
		d3dRect.y1 = l_destRect->top;
		d3dRect.x2 = l_destRect->right;
		d3dRect.y2 = l_destRect->bottom;

		return pDevice->Clear(1, &d3dRect, dx9ClearFlags, p_clearColor, 1.0f, 0);
	}
	else
	{
		// Clear entire viewport
		return pDevice->Clear(0, nullptr, dx9ClearFlags, p_clearColor, 1.0f, 0);
	}
}

HRESULT __stdcall Hook_BeginScene()
{
	if ( frameworkVar->m_pd3dDevice )
		return frameworkVar->m_pd3dDevice->BeginScene();
	else
		return -1;
}

void __stdcall Hook_EndScene()
{
	if ( frameworkVar->m_pd3dDevice )
		frameworkVar->m_pd3dDevice->EndScene();
}

HRESULT Hook_PresentFrame()
{
	IDirect3DDevice9* pDevice = frameworkVar->m_pd3dDevice;
	if ( ! pDevice )
		return 0x8200000E;

	if ( frameworkVar->m_bIsFullscreen )
	{
		// Fullscreen - present everything
		return pDevice->Present(nullptr, nullptr, nullptr, nullptr);
	}
	else
	{
		// Windowed - present with rectangles
		return pDevice->Present(
		    &frameworkVar->m_rcViewportRect, // source rect
		    &frameworkVar->m_rcScreenRect,   // dest rect
		    nullptr,                         // dest window (nullptr = use device window)
		    nullptr                          // dirty region
		);
	}
}

int32_t __cdecl Hook_SetViewport(D3DVIEWPORT2* p_viewport)
{
	IDirect3DDevice9* pDevice = frameworkVar->m_pd3dDevice;

	if ( ! pDevice )
		return -1;

	// Convert D3DVIEWPORT2 to D3DVIEWPORT9
	D3DVIEWPORT9 vp9;
	vp9.X = p_viewport->dwX;
	vp9.Y = p_viewport->dwY;
	vp9.Width = p_viewport->dwWidth;
	vp9.Height = p_viewport->dwHeight;
	vp9.MinZ = p_viewport->dvMinZ;
	vp9.MaxZ = p_viewport->dvMaxZ;

	HRESULT hr = pDevice->SetViewport(&vp9);

	return SUCCEEDED(hr) ? 0 : -1;
}

HRESULT __cdecl Hook_ReleaseVertexBuffer(LPDIRECT3DVERTEXBUFFER9 p_buffer)
{
	if ( p_buffer )
		return p_buffer->Release();

	return D3D_OK;
}

HRESULT __cdecl Hook_CreateVertexBuffer(D3DVERTEXBUFFERDESC* p_desc, LPDIRECT3DVERTEXBUFFER9* p_outBuffer, DWORD p_flags)
{
	// Just hardcoded since theres not alot of them, 0x1C4, 0x152
	if ( p_desc->dwFVF != 0x1C4 && p_desc->dwFVF != 0x152 )
		throw std::runtime_error("FVF doesn't match!");

	int32_t vertexSize = 0;

	// TODO: FIX
	if ( p_desc->dwFVF == 0x1C4 )
		vertexSize = 32;

	if ( p_desc->dwFVF == 0x152 )
		vertexSize = 36;

	UINT length = p_desc->dwNumVertices * vertexSize;
	DWORD usage = D3DUSAGE_WRITEONLY; // maybe | D3DUSAGE_DYNAMIC depending on caps
	D3DPOOL pool = D3DPOOL_DEFAULT;

	return frameworkVar->m_pd3dDevice->CreateVertexBuffer(length, usage, p_desc->dwFVF, pool, p_outBuffer, nullptr);
}

HRESULT __cdecl Hook_LockVertexBuffer(LPDIRECT3DVERTEXBUFFER9 p_vertexBuffer, DWORD p_dwFlags, LPVOID* p_lplpData, DWORD* p_lpStride)
{
	if ( ! p_vertexBuffer )
		return E_FAIL;

	// In DX9, Lock doesn't return stride - it's determined by the vertex format
	// The stride parameter is ignored in DX9
	UINT offsetToLock = 0;
	UINT sizeToLock = 0; // 0 = lock entire buffer

	// Convert D3D3 lock flags to DX9 flags
	DWORD dx9Flags = 0;

	if ( p_dwFlags & 0x800 ) // D3DLOCK_READONLY
		dx9Flags |= D3DLOCK_READONLY;
	if ( p_dwFlags & 0x1 ) // D3DLOCK_DISCARDCONTENTS or similar
		dx9Flags |= D3DLOCK_DISCARD;
	if ( p_dwFlags & 0x2000 ) // D3DLOCK_NOOVERWRITE
		dx9Flags |= D3DLOCK_NOOVERWRITE;

	return p_vertexBuffer->Lock(offsetToLock, sizeToLock, p_lplpData, dx9Flags);
}

HRESULT __cdecl Hook_UnlockVertexBuffer(LPDIRECT3DVERTEXBUFFER9 p_buffer)
{
	if ( p_buffer )
		return p_buffer->Unlock();
	return D3D_OK;
}

HRESULT __cdecl Hook_OptimizeVertexBuffer(LPDIRECT3DVERTEXBUFFER9 p_buffer, void* p_device, DWORD p_flags) { return D3D_OK; }

HRESULT __cdecl Hook_ProcessVerticesOnBuffer(
    LPDIRECT3DVERTEXBUFFER9 p_srcBuffer,
    DWORD p_dwVertexOp,
    DWORD p_dwDestIndex,
    DWORD p_dwCount,
    LPDIRECT3DVERTEXBUFFER9 p_destBuffer,
    DWORD p_dwSrcIndex,
    DWORD p_dwFlags
)
{
	IDirect3DDevice9* pDevice = frameworkVar ? frameworkVar->m_pd3dDevice : nullptr;
	if ( ! pDevice )
		return E_FAIL;

	// If this call is only asking for extents and there is no dest buffer,
	// we can just ignore it for now.
	if ( ! p_destBuffer && (p_dwVertexOp & 0x4) ) // D3DVOP_EXTENTS
		return D3D_OK;

	if ( ! p_srcBuffer || ! p_destBuffer )
		return E_FAIL;

	D3DVERTEXBUFFER_DESC srcDesc{};
	D3DVERTEXBUFFER_DESC dstDesc{};

	if ( FAILED(p_srcBuffer->GetDesc(&srcDesc)) )
		return E_FAIL;

	if ( FAILED(p_destBuffer->GetDesc(&dstDesc)) )
		return E_FAIL;

	int32_t stride = 0;

	if ( srcDesc.FVF == 0x1C4 )
		stride = 32;

	if ( srcDesc.FVF == 0x152 )
		stride = 36;

	if ( ! stride )
		return E_FAIL;

	// Bind source VB and FVF so ProcessVertices has a valid input stream.
	HRESULT hr = pDevice->SetStreamSource(0, p_srcBuffer, 0, stride);
	if ( FAILED(hr) )
		return hr;

	hr = pDevice->SetFVF(srcDesc.FVF);
	if ( FAILED(hr) )
		return hr;

	// Map D3D3 vertex op / flags to D3D9 flags.
	DWORD dx9Flags = 0;

	// D3DVOP_EXTENTS means "only compute extents, don’t copy vertices" in D3D3.
	// In D3D9 that’s D3DPV_DONOTCOPYDATA.
	if ( p_dwVertexOp & 0x4 ) // D3DVOP_EXTENTS
		dx9Flags |= D3DPV_DONOTCOPYDATA;

	// Other ops (TRANSFORM=1, LIGHT=2, CLIP=0x400) are implied in D3D9;

	hr = pDevice->ProcessVertices(
	    p_dwSrcIndex,  // SrcStartIndex
	    p_dwDestIndex, // DestIndex
	    p_dwCount,     // VertexCount
	    p_destBuffer,  // pDestBuffer
	    nullptr,       // pVertexDecl (nullptr => use FVF we just set)
	    dx9Flags
	); // Flags

	return hr;
}

HRESULT __cdecl Hook_DrawIndexedPrimitiveVB(
    D3DPRIMITIVETYPE p_primitiveType,
    LPDIRECT3DVERTEXBUFFER9 p_vertexBuffer,
    WORD* p_indices,
    DWORD p_indexCount,
    DWORD p_flags
)
{
	IDirect3DDevice9* pDevice = (IDirect3DDevice9*)frameworkVar->m_pd3dDevice;
	if ( ! pDevice || ! p_vertexBuffer )
		return E_FAIL;

	D3DVERTEXBUFFER_DESC desc;
	p_vertexBuffer->GetDesc(&desc);

	int32_t vertexSize = 0;

	if ( desc.FVF == 0x1C4 )
		vertexSize = 32;

	if ( desc.FVF == 0x152 )
		vertexSize = 36;

	UINT numVertices = desc.Size / vertexSize;

	pDevice->SetStreamSource(0, p_vertexBuffer, 0, vertexSize);

	// DX9 requires an index buffer, not a pointer
	// We need to create a temporary index buffer
	IDirect3DIndexBuffer9* pIndexBuffer = nullptr;
	HRESULT hr = pDevice->CreateIndexBuffer(p_indexCount * sizeof(WORD), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIndexBuffer, nullptr);

	if ( SUCCEEDED(hr) )
	{
		void* pData = nullptr;
		if ( SUCCEEDED(pIndexBuffer->Lock(0, 0, &pData, 0)) )
		{
			memcpy(pData, p_indices, p_indexCount * sizeof(WORD));
			pIndexBuffer->Unlock();
		}

		pDevice->SetIndices(pIndexBuffer);

		// Calculate primitive count based on type
		UINT primCount = 0;
		if ( p_primitiveType == D3DPT_TRIANGLELIST )
			primCount = p_indexCount / 3;
		else if ( p_primitiveType == D3DPT_TRIANGLESTRIP )
			primCount = p_indexCount - 2;
		else if ( p_primitiveType == D3DPT_LINELIST )
			primCount = p_indexCount / 2;
		else if ( p_primitiveType == D3DPT_LINESTRIP )
			primCount = p_indexCount - 1;

		hr = pDevice->DrawIndexedPrimitive(
		    p_primitiveType,
		    0,           // BaseVertexIndex
		    0,           // MinVertexIndex
		    numVertices, // NumVertices
		    0,           // StartIndex
		    primCount    // PrimitiveCount
		);

		pIndexBuffer->Release();
	}

	return hr;
}

HRESULT __cdecl Hook_DrawIndexedPrimitive(
    D3DPRIMITIVETYPE p_d3dptPrimitiveType,
    DWORD p_dwVertexTypeDesc,
    LPVOID p_lpvVertices,
    DWORD p_dwVertexCount,
    LPWORD p_lpwIndices,
    DWORD p_dwIndexCount,
    DWORD p_dwFlags
)
{
	IDirect3DDevice9* pDevice = (IDirect3DDevice9*)frameworkVar->m_pd3dDevice;
	if ( ! pDevice )
		return E_FAIL;

	// DX9 doesn't support DrawIndexedPrimitive with vertex pointers directly
	// We need to use DrawIndexedPrimitiveUP (User Pointer)

	// Calculate vertex stride from FVF
	UINT vertexStride = 0;
	if ( p_dwVertexTypeDesc == 0x1C4 )
		vertexStride = 32;
	else if ( p_dwVertexTypeDesc == 0x152 )
		vertexStride = 36;

	// Set FVF
	pDevice->SetFVF(p_dwVertexTypeDesc);

	// Calculate primitive count
	UINT primCount = 0;
	if ( p_d3dptPrimitiveType == D3DPT_TRIANGLELIST )
		primCount = p_dwIndexCount / 3;
	else if ( p_d3dptPrimitiveType == D3DPT_TRIANGLESTRIP )
		primCount = p_dwIndexCount - 2;
	else if ( p_d3dptPrimitiveType == D3DPT_LINELIST )
		primCount = p_dwIndexCount / 2;
	else if ( p_d3dptPrimitiveType == D3DPT_LINESTRIP )
		primCount = p_dwIndexCount - 1;

	HRESULT result = pDevice->DrawIndexedPrimitiveUP(
	    p_d3dptPrimitiveType,
	    0,               // MinVertexIndex
	    p_dwVertexCount, // NumVertices
	    primCount,       // PrimitiveCount
	    p_lpwIndices,    // pIndexData
	    D3DFMT_INDEX16,  // IndexDataFormat
	    p_lpvVertices,   // pVertexStreamZeroData
	    vertexStride     // VertexStreamZeroStride
	);

	if ( result > 0 )
	{
		std::println("Result -> {:x}", result);
	}

	return result;
}

HRESULT Hook_SetViewTransform(const D3DMATRIX* pMatrix) { return frameworkVar->m_pd3dDevice->SetTransform(D3DTS_VIEW, pMatrix); }

HRESULT Hook_SetProjectionTransform(const D3DMATRIX* pMatrix) { return frameworkVar->m_pd3dDevice->SetTransform(D3DTS_PROJECTION, pMatrix); }

HRESULT Hook_SetWorldTransform(const D3DMATRIX* pMatrix) { return frameworkVar->m_pd3dDevice->SetTransform(D3DTS_WORLD, pMatrix); }

HRESULT Hook_SetTexture(int32_t p_stageIndex, Nu3DBmpDataNode* p_bmpDataNode)
{
	if ( p_bmpDataNode )
		return frameworkVar->m_pd3dDevice->SetTexture(p_stageIndex, p_bmpDataNode->d3dTexture);

	return frameworkVar->m_pd3dDevice->SetTexture(p_stageIndex, nullptr);
}

HRESULT __cdecl Hook_SetTextureStageState(DWORD p_stage, D3DTEXTURESTAGESTATETYPE p_state, DWORD p_value)
{
	return frameworkVar->m_pd3dDevice->SetTextureStageState(p_stage, p_state, p_value);
}

static D3DBLEND ConvertOldBlend(int value)
{
    switch (value)
    {
        case 1: return D3DBLEND_ZERO;
        case 2: return D3DBLEND_ONE;
        case 3: return D3DBLEND_SRCCOLOR;
        case 4: return D3DBLEND_INVSRCCOLOR;
        case 5: return D3DBLEND_SRCALPHA;
        case 6: return D3DBLEND_INVSRCALPHA;
        case 7: return D3DBLEND_DESTALPHA;
        case 8: return D3DBLEND_INVDESTALPHA;
        case 9: return D3DBLEND_DESTCOLOR;
        case 10: return D3DBLEND_INVDESTCOLOR;
    }
    return D3DBLEND_ONE; // safe fallback
}


HRESULT __cdecl Hook_SetRenderState(D3DRENDERSTATETYPE state, DWORD value)
{
    IDirect3DDevice9* dev = frameworkVar->m_pd3dDevice;

    switch (state)
    {
        case D3DRS_SRCBLEND:
            return dev->SetRenderState(D3DRS_SRCBLEND, ConvertOldBlend(value));

        case D3DRS_DESTBLEND:
            return dev->SetRenderState(D3DRS_DESTBLEND, ConvertOldBlend(value));

        case D3DRS_ALPHABLENDENABLE:
            return dev->SetRenderState(D3DRS_ALPHABLENDENABLE, value ? TRUE : FALSE);

        case D3DRS_ZENABLE:
            return dev->SetRenderState(D3DRS_ZENABLE, value);

        case D3DRS_ZWRITEENABLE:
            return dev->SetRenderState(D3DRS_ZWRITEENABLE, value);

        case 21:
            // 4 = MODULATE
            // 3 = DECAL (used for font fades)
            //
            if (value == 4)
            {
                dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            }
            else if (value == 3)
            {
                dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
                dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            }
            return D3D_OK;

        default:
            return dev->SetRenderState(state, value);
    }
}


int32_t Hook_GlueBackdropBltFast()
{
	if ( ! Glue::g_backdrop || ! frameworkVar || ! frameworkVar->m_pd3dDevice )
		return 0;

	IDirect3DDevice9* device = frameworkVar->m_pd3dDevice;

	IDirect3DSurface9* backBuffer = nullptr;
	HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	if ( FAILED(hr) || ! backBuffer )
		return 0;

	hr = device->StretchRect(
	    Glue::g_backdrop, // src surface
	    nullptr,          // src rect (whole)
	    backBuffer,       // dst surface
	    nullptr,          // dst rect (whole)
	    D3DTEXF_NONE      // no filtering, closest to BltFast
	);

	backBuffer->Release();

	return SUCCEEDED(hr) ? 1 : 0;
}

// Stubs
void __stdcall Hook_SoftwareRenderer1()
{
	// Not needed
	return;
}

void* __cdecl Hook_BuildFont(const char* p_fontName, int32_t p_fontSize, const char* p_charSet)
{
	// Not needed
	return 0;
}

void Renderer::Init()
{
	if ( MH_Initialize() )
	{
		std::println("[Hooks]: Could not init MinHook.");
		return;
	}

	MH_CreateHook(reinterpret_cast<LPVOID>(g_initializeForWindowAddr), reinterpret_cast<LPVOID>(&Hook_InitalizeForWindow), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_createMaterialAddr), reinterpret_cast<LPVOID>(&Hook_CreateMaterial), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setMaterialAddr), reinterpret_cast<LPVOID>(&Hook_SetMaterial), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_getHandleAddr), reinterpret_cast<LPVOID>(&Hook_GetHandle), nullptr);

	// Lights
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setLightAddr), reinterpret_cast<LPVOID>(&Hook_SetLight), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_addLightAddr), reinterpret_cast<LPVOID>(&Hook_AddLight), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_createLightAddr), reinterpret_cast<LPVOID>(&Hook_CreateLight), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setLightStateAddr), reinterpret_cast<LPVOID>(&Hook_SetLightState), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_deleteLightAddr), reinterpret_cast<LPVOID>(&Hook_DeleteLight), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_releaseLightAddr), reinterpret_cast<LPVOID>(&Hook_ReleaseLight), nullptr);

	MH_CreateHook(reinterpret_cast<LPVOID>(g_intialiseTextureSurfaceAddr), reinterpret_cast<LPVOID>(&Hook_InitialiseTextureSurface), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_glueSetBackdropAddr), reinterpret_cast<LPVOID>(&Hook_GlueSetBackdrop), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_clearScreenAddr), reinterpret_cast<LPVOID>(&Hook_ClearScreen), nullptr);

	MH_CreateHook(reinterpret_cast<LPVOID>(g_beginSceneAddr), reinterpret_cast<LPVOID>(&Hook_BeginScene), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_endSceneAddr), reinterpret_cast<LPVOID>(&Hook_EndScene), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_presentFrameAddr), reinterpret_cast<LPVOID>(&Hook_PresentFrame), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setViewportAddr), reinterpret_cast<LPVOID>(&Hook_SetViewport), nullptr);

	MH_CreateHook(reinterpret_cast<LPVOID>(g_releaseVertexBufferAddr), reinterpret_cast<LPVOID>(&Hook_ReleaseVertexBuffer), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_createVertexBufferAddr), reinterpret_cast<LPVOID>(&Hook_CreateVertexBuffer), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_lockVertexBufferAddr), reinterpret_cast<LPVOID>(&Hook_LockVertexBuffer), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_unlockVertexBufferAddr), reinterpret_cast<LPVOID>(&Hook_UnlockVertexBuffer), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_optimizeVertexBufferAddr), reinterpret_cast<LPVOID>(&Hook_OptimizeVertexBuffer), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_processVertsOnBufferAddr), reinterpret_cast<LPVOID>(&Hook_ProcessVerticesOnBuffer), nullptr);

	MH_CreateHook(reinterpret_cast<LPVOID>(g_drawIndexedPrimVBAddr), reinterpret_cast<LPVOID>(&Hook_DrawIndexedPrimitiveVB), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_drawIndexedPrimAddr), reinterpret_cast<LPVOID>(&Hook_DrawIndexedPrimitive), nullptr);

	MH_CreateHook(reinterpret_cast<LPVOID>(g_setViewTransformAddr), reinterpret_cast<LPVOID>(&Hook_SetViewTransform), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setProjTransformAddr), reinterpret_cast<LPVOID>(&Hook_SetProjectionTransform), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setWorldTransformAddr), reinterpret_cast<LPVOID>(&Hook_SetWorldTransform), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setTextureAddr), reinterpret_cast<LPVOID>(&Hook_SetTexture), nullptr);

	MH_CreateHook(reinterpret_cast<LPVOID>(g_setRenderState), reinterpret_cast<LPVOID>(&Hook_SetRenderState), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_setTextureStageAddr), reinterpret_cast<LPVOID>(&Hook_SetTextureStageState), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_backdropBltFastAddr), reinterpret_cast<LPVOID>(&Hook_GlueBackdropBltFast), nullptr);

	// Stubs
	MH_CreateHook(reinterpret_cast<LPVOID>(g_softwareRenderer1Addr), reinterpret_cast<LPVOID>(&Hook_SoftwareRenderer1), nullptr);
	MH_CreateHook(reinterpret_cast<LPVOID>(g_fontBuilderAddr), reinterpret_cast<LPVOID>(&Hook_BuildFont), nullptr);

	std::println("Hook has been created.");

	if ( MH_EnableHook(MH_ALL_HOOKS) )
		std::println("[Hooks]: Could not enable all hooks.");
}

void Renderer::Render()
{
	
}
