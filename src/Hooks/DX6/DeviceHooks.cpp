#include "DeviceHooks.hpp"
#include "Mapper.hpp"
#include "Settings.hpp"

#include <print>

namespace DX6
{
	static CD3DFramework g_framework {};

	bool DeviceHooks::initialize(HWND hWnd, bool fullscreen, int32_t width, int32_t height, GUID* ddGuid, const IID& deviceGuid)
	{
		g_framework.hWnd = hWnd;
		g_framework.bIsFullscreen = fullscreen ? 1 : 0;
		g_framework.dwRenderWidth = width;
		g_framework.dwRenderHeight = height;

		// Create DirectDraw
		bool result = createDirectDrawInterface(ddGuid, fullscreen);
		if (! result)
		{
			std::println("[DX6]: createDirectDrawInterface failed.");
			goto CLEANUP_LBL;
		}

		// Create Direct3D interface
		result = createD3DInterface();
		if (! result)
		{
			std::println("[DX6]: createD3DInterface failed.");
			goto CLEANUP_LBL;
		}

		// Select device if specified
		result = selectDevice(deviceGuid);
		if (! result)
		{
			std::println("[DX6]: selectDevice failed.");
			goto CLEANUP_LBL;
		}

		// Create surfaces
		result = createSurfaces(fullscreen, width, height);
		if (! result)
		{
			std::println("[DX6]: createSurfaces failed.");
			goto CLEANUP_LBL;
		}

		// Create Z-buffer if we have a device
		result = createZBuffer();
		if (! result)
		{
			std::println("[DX6]: createZBuffer failed.");
			goto CLEANUP_LBL;
		}

		// Create device
		result = createDevice(deviceGuid);
		if (! result)
		{
			std::println("[DX6]: createDevice failed.");
			goto CLEANUP_LBL;
		}

		// Create viewport
		result = createViewport();
		if (! result)
		{
			std::println("[DX6]: createViewport failed.");
			goto CLEANUP_LBL;
		}

		// Setup slot 0 with back buffer info
		g_framework.slots[0].valid = 1;
		g_framework.slots[0].width = width;
		g_framework.slots[0].height = height;
		g_framework.slots[0].surface1 = g_framework.pddsBackBuffer;
		g_framework.slots[0].surface2 = g_framework.pddsZBuffer;

		g_framework.initialized = 1;
		return result;

	CLEANUP_LBL:
		cleanup();

		return result;
	}

	bool DeviceHooks::createDirectDrawInterface(GUID* ddGuid, bool fullscreen)
	{
		LPDIRECTDRAW pDD = nullptr;
		HRESULT hr;

		// Create DirectDraw
		hr = DirectDrawCreate(ddGuid, &pDD, nullptr);
		if (FAILED(hr))
			return false;

		// Get DirectDraw4 interface
		hr = pDD->QueryInterface(IID_IDirectDraw4, (void**)&g_framework.pDD);
		pDD->Release();

		if (FAILED(hr))
			return false;

		// Set cooperative level
		DWORD coopFlags = fullscreen ? (DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT) : (DDSCL_NORMAL);

		hr = g_framework.pDD->SetCooperativeLevel(g_framework.hWnd, coopFlags);

		if (FAILED(hr))
			return false;

		return true;
	}

	bool DeviceHooks::createD3DInterface()
	{
		HRESULT hr = g_framework.pDD->QueryInterface(IID_IDirect3D3, (void**)&g_framework.pD3D);
		return SUCCEEDED(hr);
	}

	bool DeviceHooks::selectDevice(const IID& deviceGuid)
	{
		D3DFINDDEVICESEARCH search = { 0 };
		D3DFINDDEVICERESULT result = { 0 };

		search.dwSize = sizeof(D3DFINDDEVICESEARCH);
		search.dwFlags = D3DFDS_GUID;
		search.guid = deviceGuid;

		result.dwSize = sizeof(D3DFINDDEVICERESULT);

		HRESULT hr = g_framework.pD3D->FindDevice(&search, &result);
		if (FAILED(hr))
			return false;

		// Copy device description (hardware if available, otherwise software)
		if (result.ddHwDesc.dwFlags)
		{
			memcpy(&g_framework.ddDeviceDesc, &result.ddHwDesc, sizeof(D3DDEVICEDESC));
			g_framework.dwDeviceMemType = DDSCAPS_VIDEOMEMORY;
		}
		else
		{
			memcpy(&g_framework.ddDeviceDesc, &result.ddSwDesc, sizeof(D3DDEVICEDESC));
			g_framework.dwDeviceMemType = DDSCAPS_SYSTEMMEMORY;
		}

		// Enumerate Z-buffer formats
		memset(&g_framework.ddpfZBuffer, 0, sizeof(DDPIXELFORMAT));
		g_framework.ddpfZBuffer.dwFlags = DDPF_ZBUFFER;

		g_framework.pD3D->EnumZBufferFormats(deviceGuid, DeviceHooks::enumZBufferCallback, &g_framework.ddpfZBuffer);

		if (g_framework.ddpfZBuffer.dwSize != sizeof(DDPIXELFORMAT))
			return false;

		return true;
	}

	HRESULT CALLBACK DeviceHooks::enumZBufferCallback(LPDDPIXELFORMAT format, LPVOID context)
	{
		LPDDPIXELFORMAT target = (LPDDPIXELFORMAT)context;

		if (! format || ! target)
			return D3DENUMRET_OK;

		// Only accept matching Z-buffer types
		if (format->dwFlags != target->dwFlags)
			return D3DENUMRET_OK;

		// Prefer 16-bit Z-buffers
		if (format->dwZBufferBitDepth == 16)
		{
			memcpy(target, format, sizeof(DDPIXELFORMAT));
			return D3DENUMRET_CANCEL; // Found what we want
		}

		// Accept any Z-buffer if we don't have one yet
		if (target->dwSize == 0)
			memcpy(target, format, sizeof(DDPIXELFORMAT));

		return D3DENUMRET_OK;
	}

	bool DeviceHooks::createSurfaces(bool fullscreen, int32_t width, int32_t height)
	{
		DDSURFACEDESC2 desc;
		HRESULT hr;

		if (fullscreen)
		{
			// Set display mode
			hr = g_framework.pDD->SetDisplayMode(width, height, g_settings.use32BitColors ? 32 : 16, 0, 0);

			if (FAILED(hr))
				return false;

			// Create fullscreen primary surface with flip chain
			initSurfaceDesc(&desc, DDSD_CAPS | DDSD_BACKBUFFERCOUNT, 0);
			desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;
			desc.dwBackBufferCount = 1;

			hr = g_framework.pDD->CreateSurface(&desc, &g_framework.pddsFrontBuffer, nullptr);

			if (FAILED(hr))
				return false;

			// Get back buffer
			DDSCAPS2 caps = { 0 };
			caps.dwCaps = DDSCAPS_BACKBUFFER;
			hr = g_framework.pddsFrontBuffer->GetAttachedSurface(&caps, &g_framework.pddsBackBuffer);

			if (FAILED(hr))
				return false;

			g_framework.pddsRenderTarget = g_framework.pddsBackBuffer;

			// Setup rects
			SetRect(&g_framework.rcScreenRect, 0, 0, width, height);
			SetRect(&g_framework.rcViewportRect, 0, 0, width, height);
		}
		else
		{
			// Windowed mode - create primary surface
			initSurfaceDesc(&desc, DDSD_CAPS, 0);
			desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

			hr = g_framework.pDD->CreateSurface(&desc, &g_framework.pddsFrontBuffer, nullptr);
			if (FAILED(hr))
				return false;

			// Create clipper for windowed mode
			LPDIRECTDRAWCLIPPER clipper = nullptr;
			hr = g_framework.pDD->CreateClipper(0, &clipper, nullptr);
			if (FAILED(hr))
				return false;

			clipper->SetHWnd(0, g_framework.hWnd);
			g_framework.pddsFrontBuffer->SetClipper(clipper);
			clipper->Release();

			// Create offscreen back buffer
			initSurfaceDesc(&desc, DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS, 0);
			desc.dwWidth = width;
			desc.dwHeight = height;
			desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;

			hr = g_framework.pDD->CreateSurface(&desc, &g_framework.pddsBackBuffer, nullptr);

			if (FAILED(hr))
				return false;

			g_framework.pddsRenderTarget = g_framework.pddsBackBuffer;

			// Setup rects for windowed mode
			GetClientRect(g_framework.hWnd, &g_framework.rcViewportRect);
			GetClientRect(g_framework.hWnd, &g_framework.rcScreenRect);
			ClientToScreen(g_framework.hWnd, (LPPOINT)&g_framework.rcScreenRect);
			ClientToScreen(g_framework.hWnd, (LPPOINT)&g_framework.rcScreenRect.right);
		}

		g_framework.pddsRenderTarget->AddRef();

		return true;
	}

	bool DeviceHooks::createZBuffer()
	{
		// Check if device needs Z-buffer
		if (g_framework.ddDeviceDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_ZBUFFERLESSHSR)
			return false;

		DDSURFACEDESC2 desc;
		initSurfaceDesc(&desc, DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_PIXELFORMAT, 0);

		desc.dwWidth = g_framework.dwRenderWidth;
		desc.dwHeight = g_framework.dwRenderHeight;
		desc.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | g_framework.dwDeviceMemType;
		memcpy(&desc.ddpfPixelFormat, &g_framework.ddpfZBuffer, sizeof(DDPIXELFORMAT));

		HRESULT hr = g_framework.pDD->CreateSurface(&desc, &g_framework.pddsZBuffer, nullptr);
		if (FAILED(hr))
			return false;

		// Attach Z-buffer to render target
		hr = g_framework.pddsRenderTarget->AddAttachedSurface(g_framework.pddsZBuffer);

		return SUCCEEDED(hr);
	}

	bool DeviceHooks::createDevice(const IID& deviceGuid)
	{
		DDSURFACEDESC2 desc;
		desc.dwSize = sizeof(DDSURFACEDESC2);
		g_framework.pDD->GetDisplayMode(&desc);

		if (desc.ddpfPixelFormat.dwRGBBitCount <= 8)
			return false;

		HRESULT hr = g_framework.pD3D->CreateDevice(deviceGuid, g_framework.pddsRenderTarget, &g_framework.pd3dDevice, nullptr);

		return SUCCEEDED(hr);
	}

	bool DeviceHooks::createViewport()
	{
		HRESULT hr = g_framework.pD3D->CreateViewport(&g_framework.pvViewport, nullptr);
		if (FAILED(hr))
			return false;

		hr = g_framework.pd3dDevice->AddViewport(g_framework.pvViewport);
		if (FAILED(hr))
			return false;

		D3DVIEWPORT2 viewport = {};
		viewport.dwSize = sizeof(D3DVIEWPORT2);
		viewport.dwWidth = g_framework.dwRenderWidth;
		viewport.dwHeight = g_framework.dwRenderHeight;
		viewport.dvClipX = -1.0f;
		viewport.dvClipY = 1.0f;
		viewport.dvClipWidth = 2.0f;
		viewport.dvClipHeight = 2.0f;
		viewport.dvMaxZ = 1.0f;

		hr = g_framework.pvViewport->SetViewport2(&viewport);
		if (FAILED(hr))
			return false;

		hr = g_framework.pd3dDevice->SetCurrentViewport(g_framework.pvViewport);
		return SUCCEEDED(hr);
	}

	void DeviceHooks::cleanup()
	{
		if (g_framework.pvViewport)
		{
			g_framework.pvViewport->Release();
			g_framework.pvViewport = nullptr;
		}

		if (g_framework.pd3dDevice)
		{
			g_framework.pd3dDevice->Release();
			g_framework.pd3dDevice = nullptr;
		}

		if (g_framework.pddsZBuffer)
		{
			g_framework.pddsZBuffer->Release();
			g_framework.pddsZBuffer = nullptr;
		}

		if (g_framework.pddsRenderTarget && g_framework.pddsRenderTarget != g_framework.pddsFrontBuffer)
		{
			g_framework.pddsRenderTarget->Release();
			g_framework.pddsRenderTarget = nullptr;
		}

		if (g_framework.pddsBackBuffer && ! g_framework.bIsFullscreen)
		{
			g_framework.pddsBackBuffer->Release();
			g_framework.pddsBackBuffer = nullptr;
		}

		if (g_framework.pddsFrontBuffer)
		{
			g_framework.pddsFrontBuffer->Release();
			g_framework.pddsFrontBuffer = nullptr;
		}

		if (g_framework.pD3D)
		{
			g_framework.pD3D->Release();
			g_framework.pD3D = nullptr;
		}

		if (g_framework.pDD)
		{
			g_framework.pDD->SetCooperativeLevel(g_framework.hWnd, DDSCL_NORMAL);
			g_framework.pDD->Release();
			g_framework.pDD = nullptr;
		}

		g_framework.initialized = 0;
	}

	void DeviceHooks::initSurfaceDesc(LPDDSURFACEDESC2 desc, uint32_t flags, uint32_t caps)
	{
		memset(desc, 0, sizeof(DDSURFACEDESC2));
		desc->dwSize = sizeof(DDSURFACEDESC2);
		desc->dwFlags = flags;
		desc->ddsCaps.dwCaps = caps;
	}

	void CON_STDCALL hook_RunModeSelect()
	{
		auto* wndData = Mapper::mapAddress<WindowData*>(0x134488);

		*Mapper::mapAddress<CD3DFramework**>(0x484008) = &g_framework;

		bool success = DeviceHooks::initialize(wndData->mainHwnd, g_settings.fullscreen, g_settings.width, g_settings.height, nullptr, IID_IDirect3DHALDevice);

		std::println("[ModeSelect]: Initialize returned {}", success ? "Success" : "Fail");

		return;
	}

	bool DeviceHooks::init()
	{
		// Address definitions
		const int32_t kRunModeSelect = Mapper::mapAddress(0x12B50);

		// Init Hooks
		Hook::createHook(kRunModeSelect, &hook_RunModeSelect);

		return ! Hook::hasFailedHooks();
	}
}