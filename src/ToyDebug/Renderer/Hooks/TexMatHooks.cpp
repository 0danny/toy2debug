#include "TexMatHooks.hpp"
#include "Renderer/RendererCommon.hpp"

#include <d3dtypes.h>

HRESULT CON_CDECL hook_CreateMaterial(D3DMATERIAL9** outMaterial)
{
	D3DMATERIAL9* pMaterial = new D3DMATERIAL9;
	ZeroMemory(pMaterial, sizeof(D3DMATERIAL9));

	*outMaterial = pMaterial;

	return D3D_OK;
}

HRESULT CON_CDECL hook_SetMaterial(D3DMATERIAL9* d3dMaterial9, LPD3DMATERIAL d3dMaterial)
{
	d3dMaterial9->Diffuse = d3dMaterial->diffuse;
	d3dMaterial9->Ambient = d3dMaterial->ambient;
	d3dMaterial9->Specular = d3dMaterial->specular;
	d3dMaterial9->Emissive = d3dMaterial->emissive;
	d3dMaterial9->Power = d3dMaterial->power;

	return D3D_OK;
}

HRESULT CON_CDECL hook_GetHandle(D3DMATERIAL9* d3dMaterial9, void** d3dMaterialHandle)
{
	*d3dMaterialHandle = d3dMaterial9;

	return D3D_OK;
}

void CON_CDECL copyTextureToSurface(Nu3DBmpDataNode* bitmapDataNode)
{
	LPDIRECT3DTEXTURE9 pTexture = bitmapDataNode->d3dTexture;
	if (! pTexture || ! bitmapDataNode->texData)
		return;

	D3DSURFACE_DESC surfaceDesc {};
	if (FAILED(pTexture->GetLevelDesc(0, &surfaceDesc)))
		return;

	D3DLOCKED_RECT lockedRect {};
	if (FAILED(pTexture->LockRect(0, &lockedRect, nullptr, 0)))
		return;

	const int32_t texWidth = bitmapDataNode->textureWidth;
	const int32_t texHeight = bitmapDataNode->textureHeight;

	const D3DFORMAT fmt = surfaceDesc.Format;

	for (int32_t y = 0; y < texHeight; ++y)
	{
		// D3D surfaces are top down, texData is bottom up in the original code
		int32_t srcY = texHeight - 1 - y;

		uint8_t* dstRow = (uint8_t*)lockedRect.pBits + y * lockedRect.Pitch;

		for (int32_t x = 0; x < texWidth; ++x)
		{
			uint32_t argb = bitmapDataNode->texData[srcY * texWidth + x];

			uint8_t a = (uint8_t)((argb >> 24) & 0xFF);
			uint8_t r = (uint8_t)((argb >> 16) & 0xFF);
			uint8_t g = (uint8_t)((argb >> 8) & 0xFF);
			uint8_t b = (uint8_t)(argb & 0xFF);

			if (fmt == D3DFMT_A8R8G8B8 || fmt == D3DFMT_X8R8G8B8)
			{
				uint32_t* dst32 = (uint32_t*)dstRow;
				uint32_t pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | ((uint32_t)b);

				dst32[x] = pixel;
			}
			else if (fmt == D3DFMT_A1R5G5B5)
			{
				uint16_t* dst16 = (uint16_t*)dstRow;

				uint16_t aa = (a >= 128) ? 1u : 0u;
				uint16_t rr = (uint16_t)(r >> 3);
				uint16_t gg = (uint16_t)(g >> 3);
				uint16_t bb = (uint16_t)(b >> 3);

				dst16[x] = (aa << 15) | (rr << 10) | (gg << 5) | bb;
			}
			else if (fmt == D3DFMT_A4R4G4B4)
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
				// Unkown format
			}
		}
	}

	pTexture->UnlockRect(0);
}

int32_t CON_CDECL hook_InitialiseTextureSurface(Nu3DBmpDataNode* bmpDataNode)
{
	uint32_t l_width;
	uint32_t l_height;
	int32_t l_flags;
	uint32_t* l_texData;

	if ((bmpDataNode->flags & 0x40) != 0)
		return 0;

	auto destroyBmpDataNode = [](Nu3DBmpDataNode* bmpDataNode) {
		auto l_d3dTexture = bmpDataNode->d3dTexture;

		if (l_d3dTexture)
		{
			l_d3dTexture->Release();
			bmpDataNode->d3dTexture = 0;
		}
	};

	destroyBmpDataNode(bmpDataNode);

	// Get device caps (DX9 style)
	D3DCAPS9 l_deviceCaps;
	if (FAILED(RendererCommon::g_framework->pd3dDevice->GetDeviceCaps(&l_deviceCaps)))
		return 0;

	l_width = bmpDataNode->textureWidth;
	l_height = bmpDataNode->textureHeight;
	l_flags = bmpDataNode->flags;

	D3DFORMAT textureFormat = D3DFMT_X8R8G8B8; // Default: no alpha

	if ((l_flags & 0xF) != 0)
	{
		// Need alpha channel
		int32_t minAlphaBits = (l_flags & 1) != 0 ? 4 : 1;

		if (minAlphaBits >= 4)
			textureFormat = D3DFMT_A4R4G4B4; // 4-bit alpha
		else
			textureFormat = D3DFMT_A1R5G5B5; // 1-bit alpha
	}

	LPDIRECT3DTEXTURE9 pTexture = nullptr;
	HRESULT hr = RendererCommon::g_framework->pd3dDevice->CreateTexture(l_width,
		l_height,
		1, // mip levels (1 = no mipmaps)
		0, // usage
		textureFormat,
		D3DPOOL_MANAGED, // managed pool for easy access
		&pTexture,
		nullptr);

	if (FAILED(hr) || ! pTexture)
		return 0;

	// Store texture pointer (replaces surface and d3dTexture)
	bmpDataNode->d3dTexture = pTexture;
	bmpDataNode->surface = nullptr; // Not used in DX9

	D3DSURFACE_DESC surfaceDesc;
	if (SUCCEEDED(pTexture->GetLevelDesc(0, &surfaceDesc)))
	{
		bmpDataNode->surfaceDesc.dwWidth = surfaceDesc.Width;
		bmpDataNode->surfaceDesc.dwHeight = surfaceDesc.Height;
	}

	l_texData = bmpDataNode->texData;

	if (l_texData)
		copyTextureToSurface(bmpDataNode);

	return 1;
}

HRESULT hook_SetTexture(int32_t stageIndex, Nu3DBmpDataNode* bmpDataNode)
{
	if (bmpDataNode)
		return RendererCommon::g_framework->pd3dDevice->SetTexture(stageIndex, bmpDataNode->d3dTexture);

	return RendererCommon::g_framework->pd3dDevice->SetTexture(stageIndex, nullptr);
}

HRESULT CON_CDECL hook_SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value)
{
	return RendererCommon::g_framework->pd3dDevice->SetTextureStageState(stage, state, value);
}

bool TexMatHooks::init()
{
	// Address definitions
	constexpr int32_t kCreateMaterialAddr = 0x004AC100;
	constexpr int32_t kSetMaterialAddr = 0x004AC140;
	constexpr int32_t kGetHandleAddr = 0x004AC120;
	constexpr int32_t kSetTextureStageAddr = 0x004AC150;
	constexpr int32_t kSetTextureAddr = 0x004AC1A0;
	constexpr int32_t kIntialiseTextureSurfaceAddr = 0x004B0200;

	// Init Hooks
	bool result = false;

	result = Hook::createHook(kCreateMaterialAddr, &hook_CreateMaterial);
	result = Hook::createHook(kSetMaterialAddr, &hook_SetMaterial);
	result = Hook::createHook(kGetHandleAddr, &hook_GetHandle);
	result = Hook::createHook(kSetTextureStageAddr, &hook_SetTextureStageState);
	result = Hook::createHook(kSetTextureAddr, &hook_SetTexture);
	result = Hook::createHook(kIntialiseTextureSurfaceAddr, &hook_InitialiseTextureSurface);

	return result;
}
