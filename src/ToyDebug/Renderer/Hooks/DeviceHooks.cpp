#include "DeviceHooks.hpp"
#include "Renderer/RendererCommon.hpp"

#include <print>

static bool hardcodeRes = true;

// All of the hooks for building our modified drawing device are located here
void setWindowSize(CD3DFramework* framework, DDAppDisplayMode* displayMode, uint8_t flags)
{
	if ((flags & 1) != 0)
	{
		// Fullscreen mode
		SetRect(&framework->rcViewportRect, 0, 0, displayMode->surfaceDesc.dwWidth, displayMode->surfaceDesc.dwHeight);

		framework->rcScreenRect = framework->rcViewportRect;
		framework->dwRenderWidth = displayMode->surfaceDesc.dwWidth;
		framework->dwRenderHeight = displayMode->surfaceDesc.dwHeight;
	}
	else
	{
		if (hardcodeRes)
		{
			std::println("[Debug]: Hardcoding resolution to 1600x900\n");
			SetRect(&framework->rcViewportRect, 0, 0, 1600, 900);
			SetRect(&framework->rcScreenRect, 0, 0, 1600, 900);

			framework->dwRenderWidth = 1600;
			framework->dwRenderHeight = 900;

			return;
		}

		// Windowed mode
		GetClientRect(framework->hWnd, &framework->rcViewportRect);
		GetClientRect(framework->hWnd, &framework->rcScreenRect);

		ClientToScreen(framework->hWnd, (LPPOINT)&framework->rcScreenRect);
		ClientToScreen(framework->hWnd, (LPPOINT)&framework->rcScreenRect.right);

		framework->dwRenderWidth = framework->rcViewportRect.right;
		framework->dwRenderHeight = framework->rcViewportRect.bottom;
	}
}

void setCapabilities(CD3DFramework* framework)
{
	framework->ddDeviceDesc.dpcTriCaps.dwShadeCaps = 0x4000 // alpha blending supported
		| 0x8000 // alpha gouraud
		| 0x40 // color gouraud
		| 0x20; // color flat

	framework->ddDeviceDesc.dpcTriCaps.dwDestBlendCaps = 0x0002 // D3DBLEND_ONE
		| 0x0008 // D3DBLEND_INVSRCALPHA
		| 0x0040; // SRCALPHA

	framework->ddDeviceDesc.dpcTriCaps.dwSrcBlendCaps = 0x0001 | 0x0002 | 0x0020 | 0x0040;
	framework->ddDeviceDesc.dwTextureOpCaps = 0x1 | 0x4;

	framework->ddDeviceDesc.dpcTriCaps.dwTextureFilterCaps = 0x10;

	framework->dwDeviceMemType = 0x4000; // Hardware
	framework->ddDeviceDesc.wMaxSimultaneousTextures = 1;
}

int32_t createD3DDevice(CD3DFramework* framework)
{
	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));

	d3dpp.Windowed = ! framework->bIsFullscreen;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferWidth = framework->dwRenderWidth;
	d3dpp.BackBufferHeight = framework->dwRenderHeight;
	d3dpp.BackBufferFormat = framework->bIsFullscreen ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.hDeviceWindow = framework->hWnd;

	printf("Render Width -> %d\n", framework->dwRenderWidth);
	printf("Render Height -> %d\n", framework->dwRenderHeight);

	// clang-format off
    HRESULT hr = framework->pDD->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        framework->hWnd,
        D3DCREATE_MIXED_VERTEXPROCESSING,
        &d3dpp,
        &framework->pd3dDevice);
	// clang-format on

	if (FAILED(hr))
		return false;

	// Get back buffer reference
	framework->pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &framework->pddsBackBuffer);

	// Get depth stencil surface
	framework->pd3dDevice->GetDepthStencilSurface(&framework->pddsZBuffer);

	// Set render target
	framework->pddsRenderTarget = framework->pddsBackBuffer;

	if (framework->pddsRenderTarget)
		framework->pddsRenderTarget->AddRef();

	return true;
}

int32_t createAndSetViewport(CD3DFramework* framework)
{
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width = framework->dwRenderWidth;
	vp.Height = framework->dwRenderHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;

	return SUCCEEDED(framework->pd3dDevice->SetViewport(&vp));
}

HRESULT CON_FASTCALL hook_InitalizeForWindow(CD3DFramework* framework, void* EDX, HWND hwnd, GUID* ddAppGuid, DDAppDevice* appDevice, DDAppDisplayMode* displayMode, uint8_t flags)
{
	std::println("[DeviceHooks]: InitializeForWindow!");

	RendererCommon::g_framework = framework;

	if (! hwnd || (! displayMode && (flags & 1) != 0))
		return DDERR_INVALIDPARAMS;

	flags &= ~0x1; // Clear bit 0 to disable fullscreen

	framework->hWnd = hwnd;
	framework->bIsFullscreen = flags & 1;
	framework->pDD = Direct3DCreate9(D3D_SDK_VERSION);

	if (! framework->pDD)
		return E_FAIL;

	std::println("[DeviceHooks]: Created Direct3D9 object!");

	setCapabilities(framework);
	setWindowSize(framework, displayMode, flags);

	int32_t result = createD3DDevice(framework);

	if (! result)
		return E_FAIL;

	std::println("[DeviceHooks]: Created D3D Device!");

	result = createAndSetViewport(framework);

	if (! result)
		return E_FAIL;

	if (framework->initialized)
		return E_FAIL;

	framework->slots[0].width = framework->dwRenderWidth;
	framework->slots[0].height = framework->dwRenderHeight;
	framework->slots[0].surface1 = framework->pddsBackBuffer;
	framework->slots[0].surface2 = framework->pddsZBuffer;
	framework->slots[0].valid = 1;

	framework->initialized = 1;

	// GIVE US OUR WINDOW BACK!
	if (! framework->bIsFullscreen)
	{
		LONG style = GetWindowLong(framework->hWnd, GWL_STYLE);

		// restore normal window, disable resizing
		style |= WS_OVERLAPPEDWINDOW;
		style &= ~WS_THICKFRAME;
		style &= ~WS_MAXIMIZEBOX;

		SetWindowLong(framework->hWnd, GWL_STYLE, style);

		// ensure not topmost
		SetWindowPos(framework->hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

		// correct sized window for the client area
		RECT r {};
		r.left = 0;
		r.top = 0;
		r.right = framework->dwRenderWidth;
		r.bottom = framework->dwRenderHeight;

		AdjustWindowRect(&r, style, false);

		int32_t finalWidth = r.right - r.left;
		int32_t finalHeight = r.bottom - r.top;

		SetWindowPos(framework->hWnd, nullptr, 0, 0, finalWidth, finalHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	return D3D_OK;
}

bool DeviceHooks::init()
{
	// Address definitions
	constexpr int32_t kInitializeForWindowAddr = 0x004AEDA0;

	// Init Hooks
	bool result = false;

	result = Hook::createHook(kInitializeForWindowAddr, &hook_InitalizeForWindow);

	return result;
}
