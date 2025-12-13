#include "DeviceHooks.hpp"
#include "Mapper.hpp"
#include "ToyTypes.hpp"
#include "Settings.hpp"

#define D3D_ERR_DDRAW_CREATE 0x82000001
#define D3D_ERR_COOP_LEVEL 0x82000002
#define D3D_ERR_D3D_INTERFACE 0x82000003
#define D3D_ERR_DEVICE_CREATE 0x82000004
#define D3D_ERR_ZBUFFER 0x82000005
#define D3D_ERR_VIEWPORT 0x82000006
#define D3D_ERR_PRIMARY_SURFACE 0x82000007
#define D3D_ERR_CLIPPER 0x82000008
#define D3D_ERR_DISPLAY_MODE 0x82000009
#define D3D_ERR_BACK_BUFFER 0x8200000A
#define D3D_ERR_CLEANUP 0x8200000B
#define D3D_ERR_PALETTE_MODE 0x8200000D

static CD3DFramework m_framework {};
static bool m_useDoubleBuffer = true;

HRESULT CreateDirectDrawInterface(GUID* ddGuid, bool fullscreen);
HRESULT CreateD3DInterface();
HRESULT SelectDevice(const IID&);
HRESULT CreateSurfaces(bool fullscreen, int32_t width, int32_t height);
HRESULT CreateZBuffer();
HRESULT CreateDevice(const IID&);
HRESULT CreateViewport();
void Cleanup();

// Helpers
static HRESULT CALLBACK EnumZBufferCallback(LPDDPIXELFORMAT format, LPVOID context);
void InitSurfaceDesc(DDSURFACEDESC2* desc, DWORD flags, DWORD caps);
void BuildViewportData(D3DVIEWPORT2* viewport, int32_t width, int32_t height);

HRESULT Initialize(HWND hWnd, bool fullscreen, int32_t width, int32_t height, GUID* ddGuid, const IID& deviceGuid)
{
	HRESULT hr;

	m_framework.hWnd = hWnd;
	m_framework.bIsFullscreen = fullscreen ? 1 : 0;
	m_framework.dwRenderWidth = width;
	m_framework.dwRenderHeight = height;

	// Create DirectDraw
	hr = CreateDirectDrawInterface(ddGuid, fullscreen);
	if (FAILED(hr))
		goto cleanup_on_error;

	// Create Direct3D interface
	hr = CreateD3DInterface();
	if (FAILED(hr))
		goto cleanup_on_error;

	// Select device if specified
	hr = SelectDevice(deviceGuid);
	if (FAILED(hr))
		goto cleanup_on_error;

	// Create surfaces
	hr = CreateSurfaces(fullscreen, width, height);
	if (FAILED(hr))
		goto cleanup_on_error;

	// Create Z-buffer if we have a device
	hr = CreateZBuffer();
	if (FAILED(hr))
		goto cleanup_on_error;

	hr = CreateDevice(deviceGuid);
	if (FAILED(hr))
		goto cleanup_on_error;

	hr = CreateViewport();
	if (FAILED(hr))
		goto cleanup_on_error;

	// Setup slot 0 with back buffer info
	m_framework.slots[0].valid = 1;
	m_framework.slots[0].width = width;
	m_framework.slots[0].height = height;
	m_framework.slots[0].surface1 = m_framework.pddsBackBuffer;
	m_framework.slots[0].surface2 = m_framework.pddsZBuffer;

	m_framework.initialized = 1;
	return S_OK;

cleanup_on_error:
	Cleanup();
	return hr;
}

HRESULT CreateDirectDrawInterface(GUID* ddGuid, bool fullscreen)
{
	LPDIRECTDRAW pDD = nullptr;
	HRESULT hr;

	// Create DirectDraw
	hr = DirectDrawCreate(ddGuid, &pDD, nullptr);
	if (FAILED(hr))
		return D3D_ERR_DDRAW_CREATE;

	// Get DirectDraw4 interface
	hr = pDD->QueryInterface(IID_IDirectDraw4, (void**)&m_framework.pDD);
	pDD->Release();

	if (FAILED(hr))
		return D3D_ERR_DDRAW_CREATE;

	// Set cooperative level
	DWORD coopFlags = fullscreen ? (DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT) : (DDSCL_NORMAL);

	hr = m_framework.pDD->SetCooperativeLevel(m_framework.hWnd, coopFlags);

	if (FAILED(hr))
		return D3D_ERR_COOP_LEVEL;

	return S_OK;
}

HRESULT CreateD3DInterface()
{
	HRESULT hr = m_framework.pDD->QueryInterface(IID_IDirect3D3, (void**)&m_framework.pD3D);
	return FAILED(hr) ? D3D_ERR_D3D_INTERFACE : S_OK;
}

HRESULT SelectDevice(const IID& deviceGuid)
{
	D3DFINDDEVICESEARCH search = { 0 };
	D3DFINDDEVICERESULT result = { 0 };

	search.dwSize = sizeof(D3DFINDDEVICESEARCH);
	search.dwFlags = D3DFDS_GUID;
	search.guid = deviceGuid;

	result.dwSize = sizeof(D3DFINDDEVICERESULT);

	HRESULT hr = m_framework.pD3D->FindDevice(&search, &result);
	if (FAILED(hr))
		return D3D_ERR_D3D_INTERFACE;

	// Copy device description (hardware if available, otherwise software)
	if (result.ddHwDesc.dwFlags)
	{
		memcpy(&m_framework.ddDeviceDesc, &result.ddHwDesc, sizeof(D3DDEVICEDESC));
		m_framework.dwDeviceMemType = DDSCAPS_VIDEOMEMORY;
	}
	else
	{
		memcpy(&m_framework.ddDeviceDesc, &result.ddSwDesc, sizeof(D3DDEVICEDESC));
		m_framework.dwDeviceMemType = DDSCAPS_SYSTEMMEMORY;
	}

	// Enumerate Z-buffer formats
	memset(&m_framework.ddpfZBuffer, 0, sizeof(DDPIXELFORMAT));
	m_framework.ddpfZBuffer.dwFlags = DDPF_ZBUFFER;

	m_framework.pD3D->EnumZBufferFormats(deviceGuid, EnumZBufferCallback, &m_framework.ddpfZBuffer);

	if (m_framework.ddpfZBuffer.dwSize != sizeof(DDPIXELFORMAT))
		return D3D_ERR_ZBUFFER;

	return S_OK;
}

HRESULT CALLBACK EnumZBufferCallback(LPDDPIXELFORMAT format, LPVOID context)
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

HRESULT CreateSurfaces(bool fullscreen, int32_t width, int32_t height)
{
	DDSURFACEDESC2 desc;
	HRESULT hr;

	if (fullscreen)
	{
		// Set display mode
		hr = m_framework.pDD->SetDisplayMode(width, height, g_settings.use32BitColors ? 32 : 16, 0, 0);
		if (FAILED(hr))
			return D3D_ERR_DISPLAY_MODE;

		// Create fullscreen primary surface with flip chain
		InitSurfaceDesc(&desc, DDSD_CAPS | DDSD_BACKBUFFERCOUNT, 0);
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;

		if (m_useDoubleBuffer)
			desc.dwBackBufferCount = 1;

		hr = m_framework.pDD->CreateSurface(&desc, &m_framework.pddsFrontBuffer, nullptr);
		if (FAILED(hr))
			return (hr == DDERR_OUTOFVIDEOMEMORY) ? hr : D3D_ERR_PRIMARY_SURFACE;

		if (m_useDoubleBuffer)
		{
			// Get back buffer
			DDSCAPS2 caps = { 0 };
			caps.dwCaps = DDSCAPS_BACKBUFFER;
			hr = m_framework.pddsFrontBuffer->GetAttachedSurface(&caps, &m_framework.pddsBackBuffer);
			if (FAILED(hr))
				return D3D_ERR_BACK_BUFFER;

			m_framework.pddsRenderTarget = m_framework.pddsBackBuffer;
		}
		else
		{
			m_framework.pddsRenderTarget = m_framework.pddsFrontBuffer;
		}

		// Setup rects
		SetRect(&m_framework.rcScreenRect, 0, 0, width, height);
		SetRect(&m_framework.rcViewportRect, 0, 0, width, height);
	}
	else
	{
		// Windowed mode - create primary surface
		InitSurfaceDesc(&desc, DDSD_CAPS, 0);
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

		hr = m_framework.pDD->CreateSurface(&desc, &m_framework.pddsFrontBuffer, nullptr);
		if (FAILED(hr))
			return (hr == DDERR_OUTOFVIDEOMEMORY) ? hr : D3D_ERR_PRIMARY_SURFACE;

		// Create clipper for windowed mode
		LPDIRECTDRAWCLIPPER clipper = nullptr;
		hr = m_framework.pDD->CreateClipper(0, &clipper, nullptr);
		if (FAILED(hr))
			return D3D_ERR_CLIPPER;

		clipper->SetHWnd(0, m_framework.hWnd);
		m_framework.pddsFrontBuffer->SetClipper(clipper);
		clipper->Release();

		if (m_useDoubleBuffer)
		{
			// Create offscreen back buffer
			InitSurfaceDesc(&desc, DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS, 0);
			desc.dwWidth = width;
			desc.dwHeight = height;
			desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;

			hr = m_framework.pDD->CreateSurface(&desc, &m_framework.pddsBackBuffer, nullptr);
			if (FAILED(hr))
				return (hr == DDERR_OUTOFVIDEOMEMORY) ? hr : D3D_ERR_BACK_BUFFER;

			m_framework.pddsRenderTarget = m_framework.pddsBackBuffer;
		}
		else
		{
			m_framework.pddsRenderTarget = m_framework.pddsFrontBuffer;
		}

		// Setup rects for windowed mode
		GetClientRect(m_framework.hWnd, &m_framework.rcViewportRect);
		GetClientRect(m_framework.hWnd, &m_framework.rcScreenRect);
		ClientToScreen(m_framework.hWnd, (LPPOINT)&m_framework.rcScreenRect);
		ClientToScreen(m_framework.hWnd, (LPPOINT)&m_framework.rcScreenRect.right);
	}

	m_framework.pddsRenderTarget->AddRef();
	return S_OK;
}

HRESULT CreateZBuffer()
{
	// Check if device needs Z-buffer
	if (m_framework.ddDeviceDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_ZBUFFERLESSHSR)
		return S_OK;

	DDSURFACEDESC2 desc;
	InitSurfaceDesc(&desc, DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_PIXELFORMAT, 0);

	desc.dwWidth = m_framework.dwRenderWidth;
	desc.dwHeight = m_framework.dwRenderHeight;
	desc.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | m_framework.dwDeviceMemType;
	memcpy(&desc.ddpfPixelFormat, &m_framework.ddpfZBuffer, sizeof(DDPIXELFORMAT));

	HRESULT hr = m_framework.pDD->CreateSurface(&desc, &m_framework.pddsZBuffer, nullptr);
	if (FAILED(hr))
		return (hr == DDERR_OUTOFVIDEOMEMORY) ? hr : D3D_ERR_ZBUFFER;

	// Attach Z-buffer to render target
	hr = m_framework.pddsRenderTarget->AddAttachedSurface(m_framework.pddsZBuffer);
	return FAILED(hr) ? D3D_ERR_ZBUFFER : S_OK;
}

HRESULT CreateDevice(const IID& deviceGuid)
{
	// Check color depth
	DDSURFACEDESC2 desc;
	desc.dwSize = sizeof(DDSURFACEDESC2);
	m_framework.pDD->GetDisplayMode(&desc);

	if (desc.ddpfPixelFormat.dwRGBBitCount <= 8)
		return D3D_ERR_PALETTE_MODE;

	HRESULT hr = m_framework.pD3D->CreateDevice(deviceGuid, m_framework.pddsRenderTarget, &m_framework.pd3dDevice, nullptr);

	return FAILED(hr) ? D3D_ERR_DEVICE_CREATE : S_OK;
}

HRESULT CreateViewport()
{
	HRESULT hr = m_framework.pD3D->CreateViewport(&m_framework.pvViewport, nullptr);
	if (FAILED(hr))
		return D3D_ERR_VIEWPORT;

	hr = m_framework.pd3dDevice->AddViewport(m_framework.pvViewport);
	if (FAILED(hr))
		return D3D_ERR_VIEWPORT;

	D3DVIEWPORT2 viewport;
	BuildViewportData(&viewport, m_framework.dwRenderWidth, m_framework.dwRenderHeight);

	hr = m_framework.pvViewport->SetViewport2(&viewport);
	if (FAILED(hr))
		return D3D_ERR_VIEWPORT;

	hr = m_framework.pd3dDevice->SetCurrentViewport(m_framework.pvViewport);
	return FAILED(hr) ? D3D_ERR_VIEWPORT : S_OK;
}

void Cleanup()
{
	if (m_framework.pvViewport)
	{
		m_framework.pvViewport->Release();
		m_framework.pvViewport = nullptr;
	}

	if (m_framework.pd3dDevice)
	{
		m_framework.pd3dDevice->Release();
		m_framework.pd3dDevice = nullptr;
	}

	if (m_framework.pddsZBuffer)
	{
		m_framework.pddsZBuffer->Release();
		m_framework.pddsZBuffer = nullptr;
	}

	if (m_framework.pddsRenderTarget && m_framework.pddsRenderTarget != m_framework.pddsFrontBuffer)
	{
		m_framework.pddsRenderTarget->Release();
		m_framework.pddsRenderTarget = nullptr;
	}

	if (m_framework.pddsBackBuffer && ! m_framework.bIsFullscreen)
	{
		m_framework.pddsBackBuffer->Release();
		m_framework.pddsBackBuffer = nullptr;
	}

	if (m_framework.pddsFrontBuffer)
	{
		m_framework.pddsFrontBuffer->Release();
		m_framework.pddsFrontBuffer = nullptr;
	}

	if (m_framework.pD3D)
	{
		m_framework.pD3D->Release();
		m_framework.pD3D = nullptr;
	}

	if (m_framework.pDD)
	{
		m_framework.pDD->SetCooperativeLevel(m_framework.hWnd, DDSCL_NORMAL);
		m_framework.pDD->Release();
		m_framework.pDD = nullptr;
	}

	m_framework.initialized = 0;
}

void InitSurfaceDesc(DDSURFACEDESC2* desc, DWORD flags, DWORD caps)
{
	memset(desc, 0, sizeof(DDSURFACEDESC2));
	desc->dwSize = sizeof(DDSURFACEDESC2);
	desc->dwFlags = flags;
	desc->ddsCaps.dwCaps = caps;
}

void BuildViewportData(D3DVIEWPORT2* viewport, int32_t width, int32_t height)
{
	memset(viewport, 0, sizeof(D3DVIEWPORT2));
	viewport->dwSize = sizeof(D3DVIEWPORT2);
	viewport->dwWidth = width;
	viewport->dwHeight = height;
	viewport->dvClipX = -1.0f;
	viewport->dvClipY = 1.0f;
	viewport->dvClipWidth = 2.0f;
	viewport->dvClipHeight = 2.0f;
	viewport->dvMaxZ = 1.0f;
}

void CON_STDCALL hook_RunModeSelect()
{
	WindowData* wndData = (WindowData*)(g_newImageBase + 0x134488);

	const static GUID IID_IDirect3DHALDevice = { 0x84e63de0, 0x46aa, 0x11cf, { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e } };

	*(CD3DFramework**)MAP_ADDRESS(0x484008) = &m_framework;

	HRESULT hr = Initialize(wndData->mainHwnd, g_settings.fullscreen, g_settings.width, g_settings.height, nullptr, IID_IDirect3DHALDevice);

	return;
}

bool DeviceHooks::init()
{
	// Address definitions
	const int32_t kRunModeSelect = MAP_ADDRESS(0x12B50);

	// Init Hooks
	Hook::createHook(kRunModeSelect, &hook_RunModeSelect);

	return ! Hook::hasFailedHooks();
}