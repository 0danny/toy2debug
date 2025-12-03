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
	// In DX9, depth/stencil buffers are created with the device or separately
	// For compatibility, we'll create it as part of the device setup
	// Store success for now - actual creation happens in CreateD3DDevice
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

	// In DX9, we don't need to query device caps this way
	// Just set some defaults for compatibility
	_this->m_dwDeviceMemType = 0x4000; // Hardware

	// Set up Z-buffer format info for compatibility
	memset(&_this->m_ddpfZBuffer, 0, sizeof(_this->m_ddpfZBuffer));
	_this->m_ddpfZBuffer.dwSize = 32;
	_this->m_ddpfZBuffer.dwFlags = (p_flags & 8) != 0 ? 17408 : 1024;
	_this->m_ddpfZBuffer.dwRGBBitCount = 16;

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
	// In DX9, viewport is set directly on device, not a separate object
	// Store viewport data for compatibility

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

			// You can add other mappings here if the game ever uses them:
			// case D3DLIGHTSTATE_MATERIAL:
			// case D3DLIGHTSTATE_COLORMODE:
			// etc

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
	if ( ! pTexture )
		return;

	// Get texture description to determine format
	D3DSURFACE_DESC surfaceDesc;
	if ( FAILED(pTexture->GetLevelDesc(0, &surfaceDesc)) )
		return;

	// Lock the texture
	D3DLOCKED_RECT lockedRect;
	HRESULT hr = pTexture->LockRect(0, &lockedRect, nullptr, 0);

	if ( FAILED(hr) )
	{
		// TODO: Error handling
		return;
	}

	// Calculate pixel format shifts
	Nu3DPixelFormatInfo pixelFormatInfo;
	Nu3D_CalculatePixelFormatShifts(&pixelFormatInfo, surfaceDesc.Format);

	int32_t texHeight = p_bitmapDataNode->textureHeight;
	int32_t texWidth = p_bitmapDataNode->textureWidth;

	// Copy pixel data row by row
	for ( int32_t currentRow = 0; currentRow < texHeight; ++currentRow )
	{
		uint16_t* destPixel = (uint16_t*)((uint8_t*)lockedRect.pBits + currentRow * lockedRect.Pitch);

		// Source data is stored bottom-up, so flip it
		uint32_t* sourcePixel = &p_bitmapDataNode->texData[texWidth * (texHeight - currentRow - 1)];

		for ( int32_t currentCol = 0; currentCol < texWidth; ++currentCol )
		{
			// Extract RGBA components (assuming source is BGRA format with separate components)
			uint32_t blue = sourcePixel[0];
			uint32_t green = sourcePixel[1];
			uint32_t red = sourcePixel[2];
			uint32_t alpha = sourcePixel[3];

			// Shift and mask each component
			uint16_t pixelValue = 0;

			if ( pixelFormatInfo.greenShift < 0 )
				pixelValue |= (green >> -pixelFormatInfo.greenShift) & pixelFormatInfo.greenMask;
			else
				pixelValue |= (green << pixelFormatInfo.greenShift) & pixelFormatInfo.greenMask;

			if ( pixelFormatInfo.blueShift < 0 )
				pixelValue |= (blue >> -pixelFormatInfo.blueShift) & pixelFormatInfo.blueMask;
			else
				pixelValue |= (blue << pixelFormatInfo.blueShift) & pixelFormatInfo.blueMask;

			if ( pixelFormatInfo.redShift < 0 )
				pixelValue |= (red >> -pixelFormatInfo.redShift) & pixelFormatInfo.redMask;
			else
				pixelValue |= (red << pixelFormatInfo.redShift) & pixelFormatInfo.redMask;

			// Add alpha if texture has alpha channel
			if ( (p_bitmapDataNode->flags & 0xF) != 0 )
			{
				if ( pixelFormatInfo.alphaShift < 0 )
					pixelValue |= (alpha >> -pixelFormatInfo.alphaShift) & pixelFormatInfo.alphaMask;
				else
					pixelValue |= (alpha << pixelFormatInfo.alphaShift) & pixelFormatInfo.alphaMask;
			}

			*destPixel++ = pixelValue;
			++sourcePixel;
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

	if ( texIndex )
	{
		int32_t l_result = 0;

		auto* p_bmpDataNode = texDataFreeList[texIndex].bmpDataNode;

		if ( ! p_bmpDataNode )
			return 0;

		HBITMAP l_bmpHandle = p_bmpDataNode->bitmapHandle;
		if ( ! l_bmpHandle )
			return 0;

		DIBSECTION l_dibSection;
		if ( ! GetObjectA(l_bmpHandle, sizeof(DIBSECTION), &l_dibSection) )
			return 0;

		// Get surface description
		D3DSURFACE_DESC surfaceDesc;
		if ( FAILED(p_d3dSurface->GetDesc(&surfaceDesc)) )
			return 0;

		// Lock the surface
		D3DLOCKED_RECT lockedRect;
		HRESULT hr = p_d3dSurface->LockRect(&lockedRect, nullptr, 0);
		if ( FAILED(hr) )
		{
			// TODO: Error handling
			return 0;
		}

		// Create DC for bitmap
		HDC l_dc = CreateCompatibleDC(nullptr);
		if ( ! l_dc )
		{
			p_d3dSurface->UnlockRect();
			return 0;
		}

		HGDIOBJ l_oldObject = SelectObject(l_dc, p_bmpDataNode->bitmapHandle);

		// Get color table for 8-bit bitmaps
		RGBQUAD l_colorTables[256];
		if ( l_dibSection.dsBmih.biBitCount == 8 )
			GetDIBColorTable(l_dc, 0, 256, l_colorTables);

		// Calculate pixel format shifts
		Nu3DPixelFormatInfo l_pixelFormat;
		Nu3D_CalculatePixelFormatShifts(&l_pixelFormat, surfaceDesc.Format);

		DWORD l_width = surfaceDesc.Width;
		DWORD l_height = surfaceDesc.Height;

		// Determine bits per pixel from format
		DWORD l_bitCount = 32; // Default
		if ( surfaceDesc.Format == D3DFMT_R5G6B5 || surfaceDesc.Format == D3DFMT_A1R5G5B5 || surfaceDesc.Format == D3DFMT_X1R5G5B5 ||
		     surfaceDesc.Format == D3DFMT_A4R4G4B4 )
		{
			l_bitCount = 16;
		}

		// Copy pixels row by row
		for ( DWORD l_currentRow = 0; l_currentRow < l_height; ++l_currentRow )
		{
			// Calculate source row (with scaling)
			int32_t* l_srcRowPtr =
			    (int32_t*)((uint8_t*)l_dibSection.dsBm.bmBits +
			               l_dibSection.dsBm.bmWidthBytes * (l_dibSection.dsBm.bmHeight - l_currentRow * l_dibSection.dsBm.bmHeight / l_height - 1));

			// Calculate destination row
			uint8_t* l_destRowPtr = (uint8_t*)lockedRect.pBits + l_currentRow * lockedRect.Pitch;

			if ( l_bitCount >= 15 )
			{
				if ( l_bitCount == 32 )
				{
					// 32-bit destination
					uint32_t* l_destRow32 = (uint32_t*)l_destRowPtr;

					for ( DWORD l_xPixel = 0; l_xPixel < l_width; ++l_xPixel )
					{
						BGRA l_color;
						Nu3D_GetDIBPixelColor(&l_color, &l_dibSection, l_colorTables, l_srcRowPtr, l_xPixel * l_dibSection.dsBm.bmWidth / l_width);

						// Shift and mask components
						uint32_t pixel = 0;

						if ( l_pixelFormat.greenShift < 0 )
							pixel |= (l_color.g >> -l_pixelFormat.greenShift) & l_pixelFormat.greenMask;
						else
							pixel |= (l_color.g << l_pixelFormat.greenShift) & l_pixelFormat.greenMask;

						if ( l_pixelFormat.blueShift < 0 )
							pixel |= (l_color.b >> -l_pixelFormat.blueShift) & l_pixelFormat.blueMask;
						else
							pixel |= (l_color.b << l_pixelFormat.blueShift) & l_pixelFormat.blueMask;

						if ( l_pixelFormat.redShift < 0 )
							pixel |= (l_color.r >> -l_pixelFormat.redShift) & l_pixelFormat.redMask;
						else
							pixel |= (l_color.r << l_pixelFormat.redShift) & l_pixelFormat.redMask;

						l_destRow32[l_xPixel] = pixel;
					}
				}
				else if ( l_bitCount == 16 )
				{
					// 16-bit destination
					uint16_t* l_destRow16 = (uint16_t*)l_destRowPtr;

					for ( DWORD l_xPixel = 0; l_xPixel < l_width; ++l_xPixel )
					{
						BGRA l_color;
						Nu3D_GetDIBPixelColor(&l_color, &l_dibSection, l_colorTables, l_srcRowPtr, l_xPixel * l_dibSection.dsBm.bmWidth / l_width);

						// Shift and mask components
						uint16_t pixel = 0;

						if ( l_pixelFormat.greenShift < 0 )
							pixel |= (l_color.g >> -l_pixelFormat.greenShift) & (uint16_t)l_pixelFormat.greenMask;
						else
							pixel |= (l_color.g << l_pixelFormat.greenShift) & (uint16_t)l_pixelFormat.greenMask;

						if ( l_pixelFormat.blueShift < 0 )
							pixel |= (l_color.b >> -l_pixelFormat.blueShift) & (uint16_t)l_pixelFormat.blueMask;
						else
							pixel |= (l_color.b << l_pixelFormat.blueShift) & (uint16_t)l_pixelFormat.blueMask;

						if ( l_pixelFormat.redShift < 0 )
							pixel |= (l_color.r >> -l_pixelFormat.redShift) & (uint16_t)l_pixelFormat.redMask;
						else
							pixel |= (l_color.r << l_pixelFormat.redShift) & (uint16_t)l_pixelFormat.redMask;

						l_destRow16[l_xPixel] = pixel;
					}
				}
			}

			l_result = 1;
		}

		SelectObject(l_dc, l_oldObject);
		DeleteDC(l_dc);

		p_d3dSurface->UnlockRect();

		return l_result;
	}
	else
	{
		return 0;
	}
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

	// In DX9, Present() handles both fullscreen flip and windowed blit
	// No need to check IsLost manually - Present will return the error
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
	IDirect3DDevice9* pDevice = (IDirect3DDevice9*)frameworkVar->m_pd3dDevice;
	if ( ! pDevice )
		return E_FAIL;

	// Convert vertex operation flags
	DWORD dx9Flags = 0;

	// D3DVOP_TRANSFORM = 0x1 (transform vertices)
	// D3DVOP_LIGHT = 0x2 (light vertices)
	// D3DVOP_EXTENTS = 0x4 (compute extents)
	// D3DVOP_CLIP = 0x400 (perform clipping)

	// In DX9, ProcessVertices is simpler - it always transforms
	// The flags parameter controls lighting and clipping
	if ( p_dwVertexOp & 0x2 )            // lighting
		dx9Flags |= D3DPV_DONOTCOPYDATA; // or 0, depending on needs

	return pDevice->ProcessVertices(
	    p_dwSrcIndex,  // SrcStartIndex
	    p_dwDestIndex, // DestIndex
	    p_dwCount,     // VertexCount
	    p_destBuffer,  // pDestBuffer
	    nullptr,       // pVertexDecl (nullptr = use FVF)
	    dx9Flags       // Flags
	);
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

	// Set the vertex buffer as stream source
	pDevice->SetStreamSource(0, p_vertexBuffer, 0, vertexSize); // 32 = vertex size (adjust based on FVF)

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
	if ( p_dwVertexTypeDesc == 0x1C4 ) // Your FVF
		vertexStride = 32;             // Adjust based on actual FVF
	else if ( p_dwVertexTypeDesc == 0x152 )
		vertexStride = 36; // Adjust based on actual FVF

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

HRESULT __cdecl Hook_SetRenderState(D3DRENDERSTATETYPE p_renderStateType, DWORD p_value)
{
	return frameworkVar->m_pd3dDevice->SetRenderState(p_renderStateType, p_value);
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

	// Copy whole surface: DX9 replacement for BltFast(0,0, src, nullptr, DDBLTFAST_SRCCOLORKEY)
	// (this ignores colour key; if you want colour key, you’ll need to bake alpha in Nu3D_CopyToDDSurface)
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
	// Add DX9 rendering code here
}
