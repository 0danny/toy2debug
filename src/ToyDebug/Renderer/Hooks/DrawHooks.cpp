#include "DrawHooks.hpp"
#include "Renderer/RendererCommon.hpp"

#include <d3dtypes.h>

HRESULT CON_CDECL
hook_DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitiveType, LPDIRECT3DVERTEXBUFFER9 vertexBuffer, WORD* indices, DWORD indexCount, DWORD flags)
{
	auto* device = RendererCommon::g_framework->pd3dDevice;

	if ( ! device || ! vertexBuffer )
		return E_FAIL;

	D3DVERTEXBUFFER_DESC desc;
	vertexBuffer->GetDesc(&desc);

	int32_t vertexSize = 0;

	if ( desc.FVF == 0x1C4 )
		vertexSize = 32;

	if ( desc.FVF == 0x152 )
		vertexSize = 36;

	uint32_t numVertices = desc.Size / vertexSize;

	device->SetStreamSource(0, vertexBuffer, 0, vertexSize);

	IDirect3DIndexBuffer9* pIndexBuffer = nullptr;
	HRESULT hr = device->CreateIndexBuffer(indexCount * sizeof(WORD), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIndexBuffer, nullptr);

	if ( SUCCEEDED(hr) )
	{
		void* pData = nullptr;

		if ( SUCCEEDED(pIndexBuffer->Lock(0, 0, &pData, 0)) )
		{
			memcpy(pData, indices, indexCount * sizeof(WORD));
			pIndexBuffer->Unlock();
		}

		device->SetIndices(pIndexBuffer);

		// Calculate primitive count based on type
		uint32_t primCount = 0;

		if ( primitiveType == D3DPT_TRIANGLELIST )
			primCount = indexCount / 3;
		else if ( primitiveType == D3DPT_TRIANGLESTRIP )
			primCount = indexCount - 2;
		else if ( primitiveType == D3DPT_LINELIST )
			primCount = indexCount / 2;
		else if ( primitiveType == D3DPT_LINESTRIP )
			primCount = indexCount - 1;

		hr = device->DrawIndexedPrimitive(
		    primitiveType,
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

HRESULT CON_CDECL hook_DrawIndexedPrimitive(
    D3DPRIMITIVETYPE d3dptPrimitiveType,
    DWORD dwVertexTypeDesc,
    LPVOID lpvVertices,
    DWORD dwVertexCount,
    LPWORD lpwIndices,
    DWORD dwIndexCount,
    DWORD dwFlags
)
{
	auto* device = RendererCommon::g_framework->pd3dDevice;

	if ( ! device )
		return E_FAIL;

	uint32_t vertexStride = 0;

	if ( dwVertexTypeDesc == 0x1C4 )
		vertexStride = 32;

	else if ( dwVertexTypeDesc == 0x152 )
		vertexStride = 36;

	device->SetFVF(dwVertexTypeDesc);

	uint32_t primCount = 0;

	if ( d3dptPrimitiveType == D3DPT_TRIANGLELIST )
		primCount = dwIndexCount / 3;
	else if ( d3dptPrimitiveType == D3DPT_TRIANGLESTRIP )
		primCount = dwIndexCount - 2;
	else if ( d3dptPrimitiveType == D3DPT_LINELIST )
		primCount = dwIndexCount / 2;
	else if ( d3dptPrimitiveType == D3DPT_LINESTRIP )
		primCount = dwIndexCount - 1;

	HRESULT result = device->DrawIndexedPrimitiveUP(
	    d3dptPrimitiveType,
	    0,              // MinVertexIndex
	    dwVertexCount,  // NumVertices
	    primCount,      // PrimitiveCount
	    lpwIndices,     // pIndexData
	    D3DFMT_INDEX16, // IndexDataFormat
	    lpvVertices,    // pVertexStreamZeroData
	    vertexStride    // VertexStreamZeroStride
	);

	return result;
}

HRESULT CON_CDECL hook_DrawPrimitive(D3DPRIMITIVETYPE primitiveType, DWORD vertexTypeDesc, void* vertices, DWORD vertexCount, DWORD flags)
{
	auto* device = RendererCommon::g_framework->pd3dDevice;

	uint32_t stride = 0;

	switch ( vertexTypeDesc )
	{
		case 0x1C4: stride = 32; break;
		case 0x152: stride = 36; break;
		default: return D3D_OK;
	}

	if ( stride == 0 || vertexCount == 0 )
		return D3D_OK;

	uint32_t primCount = 0;

	switch ( primitiveType )
	{
		case D3DPT_TRIANGLELIST: primCount = vertexCount / 3; break;
		case D3DPT_TRIANGLESTRIP:
		case D3DPT_TRIANGLEFAN: primCount = (vertexCount >= 2) ? (vertexCount - 2) : 0; break;
		case D3DPT_LINELIST: primCount = vertexCount / 2; break;
		case D3DPT_LINESTRIP: primCount = (vertexCount >= 1) ? (vertexCount - 1) : 0; break;
		case D3DPT_POINTLIST: primCount = vertexCount; break;

		default: return D3D_OK;
	}

	if ( primCount == 0 )
		return D3D_OK;

	device->SetFVF(vertexTypeDesc);

	return device->DrawPrimitiveUP(primitiveType, primCount, vertices, stride);
}

bool DrawHooks::init()
{
	// Address definitions
	constexpr int32_t kDrawIndexedPrimVBAddr = 0x004AC340;
	constexpr int32_t kDrawIndexedPrimAddr = 0x004AC300;
	constexpr int32_t kDrawPrimAddr = 0x004AC2A0;

	// Init Hooks
	bool result = false;

	result = Hook::createHook(kDrawIndexedPrimVBAddr, &hook_DrawIndexedPrimitiveVB);
	result = Hook::createHook(kDrawIndexedPrimAddr, &hook_DrawIndexedPrimitive);
	result = Hook::createHook(kDrawPrimAddr, &hook_DrawPrimitive);

	return result;
}
