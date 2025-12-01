#pragma once

#include <cstdint>
#include <d3d.h>
#include <ddraw.h>

// Structs

struct DrawingDeviceSlot
{
	int32_t m_valid;
	int32_t m_unkInt2;
	int32_t m_width;
	int32_t m_height;
	LPDIRECTDRAWSURFACE4 m_surface1;
	LPDIRECTDRAWSURFACE4 m_surface2;
};

struct CD3DFramework
{
	HWND m_hWnd;
	int32_t m_bIsFullscreen;
	int32_t m_dwRenderWidth;
	int32_t m_dwRenderHeight;
	RECT m_rcScreenRect;
	RECT m_rcViewportRect;
	LPDIRECTDRAWSURFACE4 m_pddsFrontBuffer;
	LPDIRECTDRAWSURFACE4 m_pddsBackBuffer;
	LPDIRECTDRAWSURFACE4 m_pddsRenderTarget;
	LPDIRECTDRAWSURFACE4 m_pddsZBuffer;
	LPDIRECT3DDEVICE3 m_pd3dDevice;
	LPDIRECT3DVIEWPORT3 m_pvViewport;
	LPDIRECTDRAW4 m_pDD;
	LPDIRECT3D3 m_pD3D;
	D3DDEVICEDESC m_ddDeviceDesc;
	int32_t m_dwDeviceMemType;
	DDPIXELFORMAT m_ddpfZBuffer;
	int32_t m_initialized;
	DrawingDeviceSlot m_slots[8];
};

struct DDAppDisplayMode
{
  DDSURFACEDESC2 surfaceDesc;
  char modeText[40];
  DDAppDisplayMode *nextDisplayMode;
};

struct DDApp;
struct DDAppDevice
{
	GUID guid;
	DDAppDevice* ref;
	char deviceName[40];
	D3DDEVICEDESC deviceDesc;
	int32_t isHardwareAccelerated;
	int32_t supportsCurrentMode;
	int32_t canRenderWindowedOnPrimary;
	DDAppDisplayMode* primaryDisplayMode;
	DDAppDisplayMode* displayModeListHead;
	DDAppDevice* nextDevice;
	DDApp* ddAppParent;
};

struct DDApp
{
  GUID guid;
  DDApp *ref;
  char driverName[40];
  char driverDesc[40];
  DDCAPS ddCaps1;
  DDCAPS ddCaps2;
  HMONITOR hMonitor;
  int vidMemFree;
  int vidMemTotal;
  int freeTextureMem;
  int totalTextureMem;
  int freeVideoMem;
  int totalVideoMem;
  DDAppDevice *primaryDevice;
  DDAppDevice *deviceListHead;
  DDApp *chainDDApp;
};

