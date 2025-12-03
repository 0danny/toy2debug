#pragma once
#include <cstdint>
#include <d3d9.h>
#include <d3d9types.h>
#include <d3d9caps.h>
#include <ddraw.h> // Still needed for DDPIXELFORMAT compatibility

struct RGBA
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct BGRA
{
  uint8_t b;
  uint8_t g;
  uint8_t r;
  uint8_t a;
};

// Structs - keeping exact same layout, only changing pointer types
struct DrawingDeviceSlot
{
	int32_t m_valid;
	int32_t m_unkInt2;
	int32_t m_width;
	int32_t m_height;
	IDirect3DSurface9* m_surface1; // Was LPDIRECTDRAWSURFACE4
	IDirect3DSurface9* m_surface2; // Was LPDIRECTDRAWSURFACE4
};

struct D3DTRANSFORMCAPS
{
  DWORD dwSize;
  DWORD dwCaps;
};

struct D3DLIGHTINGCAPS
{
  DWORD dwSize;
  DWORD dwCaps;
  DWORD dwLightingModel;
  DWORD dwNumLights;
};

struct D3DPRIMCAPS
{
  DWORD dwSize;
  DWORD dwMiscCaps;
  DWORD dwRasterCaps;
  DWORD dwZCmpCaps;
  DWORD dwSrcBlendCaps; 
  DWORD dwDestBlendCaps;
  DWORD dwAlphaCmpCaps; 
  DWORD dwShadeCaps;
  DWORD dwTextureCaps;
  DWORD dwTextureFilterCaps;
  DWORD dwTextureBlendCaps;
  DWORD dwTextureAddressCaps;
  DWORD dwStippleWidth;
  DWORD dwStippleHeight;
};

struct D3DDEVICEDESC
{
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dcmColorModel;
  DWORD dwDevCaps;
  D3DTRANSFORMCAPS dtcTransformCaps;
  BOOL bClipping;
  D3DLIGHTINGCAPS dlcLightingCaps;
  D3DPRIMCAPS dpcLineCaps;
  D3DPRIMCAPS dpcTriCaps;
  DWORD dwDeviceRenderBitDepth;
  DWORD dwDeviceZBufferBitDepth;
  DWORD dwMaxBufferSize;
  DWORD dwMaxVertexCount;
  DWORD dwMinTextureWidth;
  DWORD dwMinTextureHeight;
  DWORD dwMaxTextureWidth;
  DWORD dwMaxTextureHeight;
  DWORD dwMinStippleWidth;
  DWORD dwMaxStippleWidth;
  DWORD dwMinStippleHeight;
  DWORD dwMaxStippleHeight;
  DWORD dwMaxTextureRepeat;
  DWORD dwMaxTextureAspectRatio;
  DWORD dwMaxAnisotropy;
  float dvGuardBandLeft;
  float dvGuardBandTop;
  float dvGuardBandRight;
  float dvGuardBandBottom;
  float dvExtentsAdjust;
  DWORD dwStencilCaps;
  DWORD dwFVFCaps;
  DWORD dwTextureOpCaps;
  WORD wMaxTextureBlendStages;
  WORD wMaxSimultaneousTextures;
};

struct CD3DFramework
{
	HWND m_hWnd;
	int32_t m_bIsFullscreen;
	int32_t m_dwRenderWidth;
	int32_t m_dwRenderHeight;
	RECT m_rcScreenRect;
	RECT m_rcViewportRect;
	IDirect3DSurface9* m_pddsFrontBuffer;  // Was LPDIRECTDRAWSURFACE4
	IDirect3DSurface9* m_pddsBackBuffer;   // Was LPDIRECTDRAWSURFACE4
	IDirect3DSurface9* m_pddsRenderTarget; // Was LPDIRECTDRAWSURFACE4
	IDirect3DSurface9* m_pddsZBuffer;      // Was LPDIRECTDRAWSURFACE4
	IDirect3DDevice9* m_pd3dDevice;        // Was LPDIRECT3DDEVICE3
	void* m_pvViewport;                    // Keep as-is for compatibility (will be dummy)
	IDirect3D9* m_pDD;                     // Was LPDIRECTDRAW4 (now holds IDirect3D9)
	void* m_pD3D;                          // Keep as-is for compatibility (unused)
	D3DDEVICEDESC m_ddDeviceDesc;           // Keep for compatibility
	int32_t m_dwDeviceMemType;
	DDPIXELFORMAT m_ddpfZBuffer; // Keep for compatibility
	int32_t m_initialized;
	DrawingDeviceSlot m_slots[8];
};

struct DDAppDisplayMode
{
	DDSURFACEDESC2 surfaceDesc;
	char modeText[40];
	DDAppDisplayMode* nextDisplayMode;
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
	DDApp* ref;
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
	DDAppDevice* primaryDevice;
	DDAppDevice* deviceListHead;
	DDApp* chainDDApp;
};

struct Nu3DBmpDataNode
{
	LPDIRECTDRAWSURFACE4 surface;
	LPDIRECT3DTEXTURE9 d3dTexture;
	DDSURFACEDESC2 surfaceDesc;
	uint32_t* texData;
	uint32_t textureWidth;
	uint32_t textureHeight;
	uint32_t bitmapWidth;
	uint32_t bitmapHeight;
	HBITMAP bitmapHandle;
	int32_t unkVar1;
	int32_t unkVar2;
	int32_t unkVar3;
	int32_t unkVar4;
	char texName[80];
	int32_t unkVar5;
	int32_t flags;
	int32_t refCount;
	Nu3DBmpDataNode* next;
	Nu3DBmpDataNode* prev;
};

struct Nu3DPixelFormatInfo
{
	int32_t alphaShift;
	int32_t redShift;
	int32_t greenShift;
	int32_t blueShift;
	int32_t alphaMask;
	int32_t redMask;
	int32_t greenMask;
	int32_t blueMask;
};

struct NGNTextureData
{
	NGNTextureData* next;
	NGNTextureData* prev;
	uint32_t isTex14;
	uint32_t textureIndex;
	Nu3DBmpDataNode* bmpDataNode;
	RGBA color;
	uint32_t textureCacheIndex;
};
