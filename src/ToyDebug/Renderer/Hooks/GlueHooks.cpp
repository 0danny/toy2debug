#include "GlueHooks.hpp"
#include "Renderer/RendererCommon.hpp"

#include <d3dtypes.h>
#include <print>

namespace
{
	static int32_t g_selectedTex = 0;
	static LPDIRECT3DSURFACE9 g_backdrop = 0;

	static NGNTextureData* g_texDataFreeList = (NGNTextureData*)0x009F6224;
}

void glueReleaseBackdrop()
{
	// Clean up glue
	g_selectedTex = -1;
}

void getDIBPixelColor(BGRA* color, DIBSECTION* dibSection, RGBQUAD* colorTables, int32_t* rowBasePtr, int32_t xOffset)
{
	color->a = 0xFF; // Default alpha to opaque

	int32_t bitDepth = dibSection->dsBmih.biBitCount;

	if (bitDepth == 8)
	{
		// 8-bit indexed color
		uint8_t* pixelPtr = (uint8_t*)rowBasePtr;
		uint8_t index = pixelPtr[xOffset];
		color->b = colorTables[index].rgbBlue;
		color->g = colorTables[index].rgbGreen;
		color->r = colorTables[index].rgbRed;
	}
	else if (bitDepth == 24)
	{
		// 24-bit RGB
		uint8_t* pixelPtr = (uint8_t*)rowBasePtr + (xOffset * 3);
		color->b = pixelPtr[0];
		color->g = pixelPtr[1];
		color->r = pixelPtr[2];
	}
	else if (bitDepth == 32)
	{
		// 32-bit BGRA
		uint8_t* pixelPtr = (uint8_t*)rowBasePtr + (xOffset * 4);
		color->b = pixelPtr[0];
		color->g = pixelPtr[1];
		color->r = pixelPtr[2];
		color->a = pixelPtr[3];
	}
	else if (bitDepth == 16)
	{
		// 16-bit RGB (5-6-5 or 5-5-5)
		uint16_t* pixelPtr = (uint16_t*)rowBasePtr;
		uint16_t pixel = pixelPtr[xOffset];

		// Assume 5-6-5 format
		color->b = ((pixel & 0x001F) << 3);
		color->g = ((pixel & 0x07E0) >> 3);
		color->r = ((pixel & 0xF800) >> 8);
	}
}

bool copyToDDSurface(int32_t texIndex, LPDIRECT3DSURFACE9 d3dSurface)
{
	if (! texIndex || ! d3dSurface)
		return false;

	auto* bmpDataNode = g_texDataFreeList[texIndex].bmpDataNode;

	if (! bmpDataNode)
		return false;

	HBITMAP l_bmpHandle = bmpDataNode->bitmapHandle;

	if (! l_bmpHandle)
		return false;

	DIBSECTION l_dibSection {};
	if (! GetObjectA(l_bmpHandle, sizeof(DIBSECTION), &l_dibSection))
		return false;

	// Surface info
	D3DSURFACE_DESC surfaceDesc {};
	if (FAILED(d3dSurface->GetDesc(&surfaceDesc)))
		return false;

	const D3DFORMAT fmt = surfaceDesc.Format;

	// Lock
	D3DLOCKED_RECT lockedRect {};
	if (FAILED(d3dSurface->LockRect(&lockedRect, nullptr, 0)))
		return false;

	// DC for palette reads
	HDC l_dc = CreateCompatibleDC(nullptr);
	if (! l_dc)
	{
		d3dSurface->UnlockRect();
		return false;
	}

	HGDIOBJ l_oldObject = SelectObject(l_dc, bmpDataNode->bitmapHandle);

	RGBQUAD l_colorTables[256] {};

	if (l_dibSection.dsBmih.biBitCount == 8)
		GetDIBColorTable(l_dc, 0, 256, l_colorTables);

	const DWORD dstWidth = surfaceDesc.Width;
	const DWORD dstHeight = surfaceDesc.Height;

	bool ok = true;

	for (DWORD y = 0; y < dstHeight; ++y)
	{
		// Source row (bottom up)
		int32_t srcRow = l_dibSection.dsBm.bmHeight - 1 - (int32_t)(y * l_dibSection.dsBm.bmHeight / dstHeight);

		int32_t* srcRowPtr = (int32_t*)((uint8_t*)l_dibSection.dsBm.bmBits + l_dibSection.dsBm.bmWidthBytes * srcRow);

		uint8_t* dstRowPtr = (uint8_t*)lockedRect.pBits + y * lockedRect.Pitch;

		for (DWORD x = 0; x < dstWidth; ++x)
		{
			// Sample from source image with scaling
			int32_t srcX = (int32_t)(x * l_dibSection.dsBm.bmWidth / dstWidth);

			BGRA c {};
			getDIBPixelColor(&c, &l_dibSection, l_colorTables, srcRowPtr, srcX);

			if (fmt == D3DFMT_X8R8G8B8 || fmt == D3DFMT_A8R8G8B8)
			{
				// Pack to 0xAARRGGBB (alpha full)
				uint32_t* dst32 = (uint32_t*)dstRowPtr;
				uint32_t pixel = (0xFFu << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | ((uint32_t)c.b);

				dst32[x] = pixel;
			}
			else if (fmt == D3DFMT_R5G6B5)
			{
				uint16_t* dst16 = (uint16_t*)dstRowPtr;

				uint16_t r = (uint16_t)(c.r >> 3); // 5 bits
				uint16_t g = (uint16_t)(c.g >> 2); // 6 bits
				uint16_t b = (uint16_t)(c.b >> 3); // 5 bits

				dst16[x] = (r << 11) | (g << 5) | b;
			}
			else if (fmt == D3DFMT_A1R5G5B5)
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

		if (! ok)
			break;
	}

	SelectObject(l_dc, l_oldObject);
	DeleteDC(l_dc);

	d3dSurface->UnlockRect();

	return ok;
}

int32_t CON_CDECL hook_GlueSetBackdrop(int32_t textureIndex)
{
	typedef int32_t(CON_CDECL * GetTextureDataIndex)(int32_t textureIndex);
	GetTextureDataIndex getTextureDataIndex = (GetTextureDataIndex)0x004CE2C0;

	int32_t l_texIndex = getTextureDataIndex(textureIndex);

	if (l_texIndex == g_selectedTex)
		return 1;

	glueReleaseBackdrop();

	g_selectedTex = l_texIndex;

	typedef HBITMAP(CON_CDECL * GetBmpHandle)(int32_t textureIndex);
	GetBmpHandle getBmpHandle = (GetBmpHandle)0x004BB6C0;

	HBITMAP l_bmpHandle = getBmpHandle(l_texIndex);

	if (! l_bmpHandle)
		return 0;

	auto* pDevice = RendererCommon::g_framework->pd3dDevice;
	if (! pDevice)
		return 0;

	// Get back buffer surface description to match dimensions
	LPDIRECT3DSURFACE9 pBackBuffer = nullptr;
	pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

	if (! pBackBuffer)
		return 0;

	D3DSURFACE_DESC backBufferDesc;
	pBackBuffer->GetDesc(&backBufferDesc);
	pBackBuffer->Release();

	// Create a system memory surface that we can lock
	LPDIRECT3DSURFACE9 pSystemSurface = nullptr;
	HRESULT hr = pDevice->CreateOffscreenPlainSurface(backBufferDesc.Width,
		backBufferDesc.Height,
		backBufferDesc.Format,
		D3DPOOL_SYSTEMMEM, // System memory can be locked
		&pSystemSurface,
		nullptr);

	if (FAILED(hr))
	{
		std::printf("Failed to create system surface -> 0x%X\n", hr);
		return 0;
	}

	// Copy bitmap data to the system memory surface
	if (! copyToDDSurface(l_texIndex, pSystemSurface))
	{
		pSystemSurface->Release();
		return 0;
	}

	// Now create the final DEFAULT pool surface for StretchRect
	LPDIRECT3DSURFACE9 pDefaultSurface = nullptr;
	hr = pDevice->CreateOffscreenPlainSurface(backBufferDesc.Width, backBufferDesc.Height, backBufferDesc.Format, D3DPOOL_DEFAULT, &pDefaultSurface, nullptr);

	if (FAILED(hr))
	{
		std::printf("Failed to create default surface -> 0x%X\n", hr);
		pSystemSurface->Release();
		return 0;
	}

	// Copy from system memory to video memory using UpdateSurface
	hr = pDevice->UpdateSurface(pSystemSurface, nullptr, pDefaultSurface, nullptr);

	pSystemSurface->Release(); // Done with system memory surface

	if (FAILED(hr))
	{
		std::printf("Failed UpdateSurface -> 0x%X\n", hr);
		pDefaultSurface->Release();
		return 0;
	}

	g_backdrop = pDefaultSurface;

	return 1;
}

int32_t hook_GlueBackdropBltFast()
{
	if (! g_backdrop || ! RendererCommon::g_framework || ! RendererCommon::g_framework->pd3dDevice)
		return 0;

	auto* device = RendererCommon::g_framework->pd3dDevice;

	IDirect3DSurface9* backBuffer = nullptr;
	HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	if (FAILED(hr) || ! backBuffer)
		return 0;

	hr = device->StretchRect(g_backdrop, // src surface
		nullptr, // src rect (whole)
		backBuffer, // dst surface
		nullptr, // dst rect (whole)
		D3DTEXF_NONE // no filtering, closest to BltFast
	);

	backBuffer->Release();

	return SUCCEEDED(hr) ? 1 : 0;
}

bool GlueHooks::init()
{
	// Address definitions
	constexpr int32_t kGlueSetBackdropAddr = 0x004CE380;
	constexpr int32_t kBackdropBltFastAddr = 0x004CE4D0;

	// Init Hooks
	bool result = false;

	result = Hook::createHook(kGlueSetBackdropAddr, &hook_GlueSetBackdrop);
	result = Hook::createHook(kBackdropBltFastAddr, &hook_GlueBackdropBltFast);

	return result;
}
