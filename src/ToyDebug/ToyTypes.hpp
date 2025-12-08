#pragma once

#include <cstdint>
#include <d3d9.h>
#include <d3d9types.h>
#include <d3d9caps.h>
#include <ddraw.h>
#include <d3d.h>
#include <d3dtypes.h>
#include <d3dcaps.h>

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

struct DrawingDeviceSlot
{
	int32_t valid;
	int32_t unkInt2;
	int32_t width;
	int32_t height;
	IDirect3DSurface9* surface1;
	IDirect3DSurface9* surface2;
};

struct CD3DFramework
{
	HWND hWnd;
	int32_t bIsFullscreen;
	int32_t dwRenderWidth;
	int32_t dwRenderHeight;
	RECT rcScreenRect;
	RECT rcViewportRect;
	IDirect3DSurface9* pddsFrontBuffer;
	IDirect3DSurface9* pddsBackBuffer;
	IDirect3DSurface9* pddsRenderTarget;
	IDirect3DSurface9* pddsZBuffer;
	IDirect3DDevice9* pd3dDevice;
	void* pvViewport;
	IDirect3D9* pDD;
	void* pD3D;
	D3DDEVICEDESC ddDeviceDesc;
	int32_t dwDeviceMemType;
	DDPIXELFORMAT ddpfZBuffer;
	int32_t initialized;
	DrawingDeviceSlot slots[8];
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
	int32_t vidMemFree;
	int32_t vidMemTotal;
	int32_t freeTextureMem;
	int32_t totalTextureMem;
	int32_t freeVideoMem;
	int32_t totalVideoMem;
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
