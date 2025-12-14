#include "PipelineHooks.hpp"
#include "Hooks/RendererCommon.hpp"

#include <d3dtypes.h>

HRESULT CON_CDECL hook_ClearScreen(DWORD clearFlags, D3DCOLOR clearColor)
{
	auto* device = RendererCommon::g_framework->pd3dDevice;

	if (! device)
		return -1;

	// Convert D3D3 clear flags to DX9 flags
	DWORD dx9ClearFlags = 0;

	// D3DCLEAR_TARGET = 1 in both D3D3 and DX9
	if (clearFlags & D3DCLEAR_TARGET)
		dx9ClearFlags |= D3DCLEAR_TARGET;

	// D3DCLEAR_ZBUFFER = 2 in both D3D3 and DX9
	if (clearFlags & D3DCLEAR_ZBUFFER)
		dx9ClearFlags |= D3DCLEAR_ZBUFFER;

	typedef RECT*(CON_CDECL * GetDestRect)();
	GetDestRect getDestRect = (GetDestRect)0x004ABD80;

	RECT* l_destRect = getDestRect();

	if (l_destRect)
	{
		// Clear specific rectangle
		D3DRECT d3dRect;
		d3dRect.x1 = l_destRect->left;
		d3dRect.y1 = l_destRect->top;
		d3dRect.x2 = l_destRect->right;
		d3dRect.y2 = l_destRect->bottom;

		return device->Clear(1, &d3dRect, dx9ClearFlags, clearColor, 1.0f, 0);
	}
	else
	{
		// Clear entire viewport
		return device->Clear(0, nullptr, dx9ClearFlags, clearColor, 1.0f, 0);
	}
}

HRESULT CON_STDCALL hook_BeginScene()
{
	if (RendererCommon::g_framework->pd3dDevice)
		return RendererCommon::g_framework->pd3dDevice->BeginScene();
	else
		return -1;
}

void CON_STDCALL hook_EndScene()
{
	if (RendererCommon::g_framework->pd3dDevice)
		RendererCommon::g_framework->pd3dDevice->EndScene();
}

HRESULT hook_PresentFrame()
{
	auto* device = RendererCommon::g_framework->pd3dDevice;

	if (! device)
		return E_FAIL;

	if (RendererCommon::g_framework->bIsFullscreen)
	{
		// Fullscreen - present everything
		return device->Present(nullptr, nullptr, nullptr, nullptr);
	}
	else
	{
		// Windowed - present with rectangles
		return device->Present(&RendererCommon::g_framework->rcViewportRect, // source rect
			&RendererCommon::g_framework->rcScreenRect, // dest rect
			nullptr, // dest window (nullptr = use device window)
			nullptr // dirty region
		);
	}
}

int32_t CON_CDECL hook_SetViewport(D3DVIEWPORT2* viewport)
{
	if (! RendererCommon::g_framework->pd3dDevice)
		return -1;

	// Convert D3DVIEWPORT2 to D3DVIEWPORT9
	D3DVIEWPORT9 vp9;
	vp9.X = viewport->dwX;
	vp9.Y = viewport->dwY;
	vp9.Width = viewport->dwWidth;
	vp9.Height = viewport->dwHeight;
	vp9.MinZ = viewport->dvMinZ;
	vp9.MaxZ = viewport->dvMaxZ;

	HRESULT hr = RendererCommon::g_framework->pd3dDevice->SetViewport(&vp9);

	return SUCCEEDED(hr) ? 0 : -1;
}

HRESULT hook_SetViewTransform(const D3DMATRIX* pMatrix) { return RendererCommon::g_framework->pd3dDevice->SetTransform(D3DTS_VIEW, pMatrix); }

HRESULT hook_SetProjectionTransform(const D3DMATRIX* pMatrix) { return RendererCommon::g_framework->pd3dDevice->SetTransform(D3DTS_PROJECTION, pMatrix); }

HRESULT hook_SetWorldTransform(const D3DMATRIX* pMatrix) { return RendererCommon::g_framework->pd3dDevice->SetTransform(D3DTS_WORLD, pMatrix); }

// This probably needs to be fixed
HRESULT CON_CDECL hook_SetRenderState(D3DRENDERSTATETYPE state, DWORD value)
{
	auto* device = RendererCommon::g_framework->pd3dDevice;

	auto convertOldBlend = [](int32_t value) -> D3DBLEND {
		switch (value)
		{
			case 1:
				return D3DBLEND_ZERO;
			case 2:
				return D3DBLEND_ONE;
			case 3:
				return D3DBLEND_SRCCOLOR;
			case 4:
				return D3DBLEND_INVSRCCOLOR;
			case 5:
				return D3DBLEND_SRCALPHA;
			case 6:
				return D3DBLEND_INVSRCALPHA;
			case 7:
				return D3DBLEND_DESTALPHA;
			case 8:
				return D3DBLEND_INVDESTALPHA;
			case 9:
				return D3DBLEND_DESTCOLOR;
			case 10:
				return D3DBLEND_INVDESTCOLOR;
		}

		return D3DBLEND_ONE; // safe fallback
	};

	switch (state)
	{
		case D3DRS_SRCBLEND:
			return device->SetRenderState(D3DRS_SRCBLEND, convertOldBlend(value));

		case D3DRS_DESTBLEND:
			return device->SetRenderState(D3DRS_DESTBLEND, convertOldBlend(value));

		case D3DRS_ALPHABLENDENABLE:
			return device->SetRenderState(D3DRS_ALPHABLENDENABLE, value ? TRUE : FALSE);

		case D3DRS_ZENABLE:
			return device->SetRenderState(D3DRS_ZENABLE, value);

		case D3DRS_ZWRITEENABLE:
			return device->SetRenderState(D3DRS_ZWRITEENABLE, value);

		case 21:
			// 4 = MODULATE
			// 3 = DECAL (used for font fades)
			//
			if (value == 4)
			{
				device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			}
			else if (value == 3)
			{
				device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
				device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			}
			return D3D_OK;

		default:
			return device->SetRenderState(state, value);
	}
}

bool PipelineHooks::init()
{
	// Address definitions
	constexpr int32_t kBeginSceneAddr = 0x004ABA90;
	constexpr int32_t kEndSceneAddr = 0x004ABAD0;
	constexpr int32_t kPresentFrameAddr = 0x004ABD40;
	constexpr int32_t kSetViewportAddr = 0x004ABB30;
	constexpr int32_t kClearScreenAddr = 0x004ABAF0;
	constexpr int32_t kSetViewTransformAddr = 0x004ABFF0;
	constexpr int32_t kSetProjTransformAddr = 0x004AC010;
	constexpr int32_t kSetWorldTransformAddr = 0x004ABFD0;
	constexpr int32_t kSetRenderState = 0x004AC370;

	// Init Hooks
	bool result = true;

	Hook::createHook(kBeginSceneAddr, &hook_BeginScene);
	Hook::createHook(kEndSceneAddr, &hook_EndScene);
	Hook::createHook(kPresentFrameAddr, &hook_PresentFrame);
	Hook::createHook(kSetViewportAddr, &hook_SetViewport);
	Hook::createHook(kClearScreenAddr, &hook_ClearScreen);
	Hook::createHook(kSetViewTransformAddr, &hook_SetViewTransform);
	Hook::createHook(kSetProjTransformAddr, &hook_SetProjectionTransform);
	Hook::createHook(kSetWorldTransformAddr, &hook_SetWorldTransform);
	Hook::createHook(kSetRenderState, &hook_SetRenderState);

	return result;
}
