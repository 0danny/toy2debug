#include "VBHooks.hpp"
#include "Renderer/RendererCommon.hpp"

#include <stdexcept>
#include <d3dtypes.h>

HRESULT CON_CDECL hook_ReleaseVertexBuffer(LPDIRECT3DVERTEXBUFFER9 buffer)
{
	if ( buffer )
		return buffer->Release();

	return D3D_OK;
}

HRESULT CON_CDECL hook_CreateVertexBuffer(D3DVERTEXBUFFERDESC* desc, LPDIRECT3DVERTEXBUFFER9* outBuffer, DWORD flags)
{
	// Just hardcoded since theres not alot of them, 0x1C4, 0x152
	if ( desc->dwFVF != 0x1C4 && desc->dwFVF != 0x152 )
		throw std::runtime_error("FVF doesn't match!");

	int32_t vertexSize = 0;

	// TODO: FIX
	if ( desc->dwFVF == 0x1C4 )
		vertexSize = 32;

	if ( desc->dwFVF == 0x152 )
		vertexSize = 36;

	uint32_t length = desc->dwNumVertices * vertexSize;

	DWORD usage = D3DUSAGE_WRITEONLY;
	D3DPOOL pool = D3DPOOL_DEFAULT;

	return RendererCommon::g_framework->pd3dDevice->CreateVertexBuffer(length, usage, desc->dwFVF, pool, outBuffer, nullptr);
}

HRESULT CON_CDECL hook_LockVertexBuffer(LPDIRECT3DVERTEXBUFFER9 vertexBuffer, DWORD dwFlags, LPVOID* lplpData, DWORD* lpStride)
{
	if ( ! vertexBuffer )
		return E_FAIL;

	// In DX9, Lock doesn't return stride - it's determined by the vertex format
	// The stride parameter is ignored in DX9
	uint32_t offsetToLock = 0;
	uint32_t sizeToLock = 0; // 0 = lock entire buffer

	DWORD dx9Flags = 0;

	if ( dwFlags & 0x800 ) // D3DLOCK_READONLY
		dx9Flags |= D3DLOCK_READONLY;

	if ( dwFlags & 0x1 ) // D3DLOCK_DISCARDCONTENTS or similar
		dx9Flags |= D3DLOCK_DISCARD;

	if ( dwFlags & 0x2000 ) // D3DLOCK_NOOVERWRITE
		dx9Flags |= D3DLOCK_NOOVERWRITE;

	return vertexBuffer->Lock(offsetToLock, sizeToLock, lplpData, dx9Flags);
}

HRESULT CON_CDECL hook_UnlockVertexBuffer(LPDIRECT3DVERTEXBUFFER9 buffer)
{
	if ( buffer )
		return buffer->Unlock();

	return D3D_OK;
}

HRESULT CON_CDECL hook_OptimizeVertexBuffer(LPDIRECT3DVERTEXBUFFER9 buffer, void* device, DWORD flags)
{
	// Don't need
	return D3D_OK;
}

HRESULT CON_CDECL hook_ProcessVerticesOnBuffer(
    LPDIRECT3DVERTEXBUFFER9 srcBuffer,
    DWORD dwVertexOp,
    DWORD dwDestIndex,
    DWORD dwCount,
    LPDIRECT3DVERTEXBUFFER9 destBuffer,
    DWORD dwSrcIndex,
    DWORD dwFlags
)
{
	if ( ! RendererCommon::g_framework || ! RendererCommon::g_framework->pd3dDevice )
		return D3D_OK;

	if ( ! destBuffer )
		return D3D_OK;

	if ( ! srcBuffer || ! destBuffer || dwCount == 0 )
		return D3D_OK;

	D3DVERTEXBUFFER_DESC srcDesc{};
	D3DVERTEXBUFFER_DESC dstDesc{};

	if ( FAILED(srcBuffer->GetDesc(&srcDesc)) )
		return D3D_OK;

	if ( FAILED(destBuffer->GetDesc(&dstDesc)) )
		return D3D_OK;

	// Figure out strides from FVF
	uint32_t srcStride = 0;
	uint32_t dstStride = 0;

	switch ( srcDesc.FVF )
	{
		case 0x1C4: srcStride = 32; break;
		case 0x152: srcStride = 36; break;
		default: break;
	}

	switch ( dstDesc.FVF )
	{
		case 0x1C4: dstStride = 32; break;
		case 0x152: dstStride = 36; break;
		default: break;
	}

	if ( ! srcStride || ! dstStride )
		return D3D_OK;

	void* srcPtr = nullptr;
	void* dstPtr = nullptr;

	if ( FAILED(srcBuffer->Lock(0, 0, &srcPtr, D3DLOCK_READONLY)) )
		return D3D_OK;

	if ( FAILED(destBuffer->Lock(0, 0, &dstPtr, 0)) )
	{
		srcBuffer->Unlock();
		return D3D_OK;
	}

	uint8_t* srcBytes = static_cast<uint8_t*>(srcPtr);
	uint8_t* dstBytes = static_cast<uint8_t*>(dstPtr);

	const uint32_t srcCapacity = srcDesc.Size / srcStride;
	const uint32_t dstCapacity = dstDesc.Size / dstStride;

	if ( dwSrcIndex >= srcCapacity || dwDestIndex >= dstCapacity )
	{
		destBuffer->Unlock();
		srcBuffer->Unlock();
		return D3D_OK;
	}

	uint32_t maxCount = dwCount;

	if ( dwSrcIndex + maxCount > srcCapacity )
		maxCount = srcCapacity - dwSrcIndex;

	if ( dwDestIndex + maxCount > dstCapacity )
		maxCount = dstCapacity - dwDestIndex;

	if ( maxCount == 0 )
	{
		destBuffer->Unlock();
		srcBuffer->Unlock();
		return D3D_OK;
	}

	const bool sameBuffer = (srcBuffer == destBuffer) && (srcStride == dstStride);

	const uint32_t copyStride = (srcDesc.FVF == dstDesc.FVF) ? srcStride : (srcStride < dstStride ? srcStride : dstStride);

	if ( sameBuffer )
	{
		// Handle potential overlap safely using order based on indices
		if ( dwDestIndex > dwSrcIndex )
		{
			// Copy backwards
			for ( int32_t i = (int32_t)maxCount - 1; i >= 0; --i )
			{
				uint8_t* s = srcBytes + (dwSrcIndex + i) * srcStride;
				uint8_t* d = dstBytes + (dwDestIndex + i) * dstStride;
				std::memmove(d, s, copyStride);
			}
		}
		else
		{
			// Copy forwards
			for ( int32_t i = 0; i < maxCount; ++i )
			{
				uint8_t* s = srcBytes + (dwSrcIndex + i) * srcStride;
				uint8_t* d = dstBytes + (dwDestIndex + i) * dstStride;
				std::memmove(d, s, copyStride);
			}
		}
	}
	else
	{
		// Different buffers – no overlap concerns
		for ( int32_t i = 0; i < maxCount; ++i )
		{
			uint8_t* s = srcBytes + (dwSrcIndex + i) * srcStride;
			uint8_t* d = dstBytes + (dwDestIndex + i) * dstStride;
			std::memcpy(d, s, copyStride);
		}
	}

	destBuffer->Unlock();
	srcBuffer->Unlock();

	return D3D_OK;
}

bool VBHooks::init()
{
	// Address definitions
	constexpr int32_t kReleaseVertexBufferAddr = 0x004AC070;
	constexpr int32_t kCreateVertexBufferAddr = 0x004AC080;
	constexpr int32_t kLockVertexBufferAddr = 0x004AC0A0;
	constexpr int32_t kUnlockVertexBufferAddr = 0x004AC0C0;
	constexpr int32_t kOptimizeVertexBufferAddr = 0x004AC0D0;
	constexpr int32_t kProcessVertsOnBufferAddr = 0x004AC030;

	// Init Hooks
	bool result = false;

	result = Hook::createHook(kReleaseVertexBufferAddr, &hook_ReleaseVertexBuffer);
	result = Hook::createHook(kCreateVertexBufferAddr, &hook_CreateVertexBuffer);
	result = Hook::createHook(kLockVertexBufferAddr, &hook_LockVertexBuffer);
	result = Hook::createHook(kUnlockVertexBufferAddr, &hook_UnlockVertexBuffer);
	result = Hook::createHook(kOptimizeVertexBufferAddr, &hook_OptimizeVertexBuffer);
	result = Hook::createHook(kProcessVertsOnBufferAddr, &hook_ProcessVerticesOnBuffer);

	return result;
}
