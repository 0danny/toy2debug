#include "VBHooks.hpp"
#include "Hooks/RendererCommon.hpp"

#include <stdexcept>
#include <d3dtypes.h>
#include <print>

HRESULT CON_CDECL hook_ReleaseVertexBuffer(LPDIRECT3DVERTEXBUFFER9 buffer)
{
	if (buffer)
		return buffer->Release();

	return D3D_OK;
}

HRESULT CON_CDECL hook_CreateVertexBuffer(D3DVERTEXBUFFERDESC* desc, LPDIRECT3DVERTEXBUFFER9* outBuffer, DWORD flags)
{
	int32_t vertexSize = RendererCommon::getStrideFromFVF(desc->dwFVF);

	uint32_t length = desc->dwNumVertices * vertexSize;

	DWORD usage = D3DUSAGE_WRITEONLY | D3DUSAGE_SOFTWAREPROCESSING;
	D3DPOOL pool = D3DPOOL_DEFAULT;

	return RendererCommon::g_framework->pd3dDevice->CreateVertexBuffer(length, usage, desc->dwFVF, pool, outBuffer, nullptr);
}

HRESULT CON_CDECL hook_LockVertexBuffer(LPDIRECT3DVERTEXBUFFER9 vertexBuffer, DWORD dwFlags, LPVOID* lplpData, DWORD* lpStride)
{
	if (! vertexBuffer)
		return E_FAIL;

	// In DX9, Lock doesn't return stride - it's determined by the vertex format
	// The stride parameter is ignored in DX9
	uint32_t offsetToLock = 0;
	uint32_t sizeToLock = 0; // 0 = lock entire buffer

	DWORD dx9Flags = 0;

	if (dwFlags & 0x800) // D3DLOCK_READONLY
		dx9Flags |= D3DLOCK_READONLY;

	if (dwFlags & 0x1) // D3DLOCK_DISCARDCONTENTS or similar
		dx9Flags |= D3DLOCK_DISCARD;

	if (dwFlags & 0x2000) // D3DLOCK_NOOVERWRITE
		dx9Flags |= D3DLOCK_NOOVERWRITE;

	return vertexBuffer->Lock(offsetToLock, sizeToLock, lplpData, dx9Flags);
}

HRESULT CON_CDECL hook_UnlockVertexBuffer(LPDIRECT3DVERTEXBUFFER9 buffer)
{
	if (buffer)
		return buffer->Unlock();

	return D3D_OK;
}

HRESULT CON_CDECL hook_OptimizeVertexBuffer(LPDIRECT3DVERTEXBUFFER9 buffer, void* device, DWORD flags)
{
	// Don't need
	return D3D_OK;
}

HRESULT CON_CDECL
hook_ProcessVerticesOnBuffer(LPDIRECT3DVERTEXBUFFER9 destBuffer, DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER9 srcBuffer, DWORD dwSrcIndex, DWORD dwFlags)
{
	auto* device = RendererCommon::g_framework->pd3dDevice;

	if (! device || ! srcBuffer || ! destBuffer || dwCount == 0)
		return D3D_OK;

	D3DVERTEXBUFFER_DESC srcDesc {};
	if (FAILED(srcBuffer->GetDesc(&srcDesc)))
		return D3D_OK;

	D3DVERTEXBUFFER_DESC destDesc {};
	if (FAILED(destBuffer->GetDesc(&destDesc)))
		return D3D_OK;

	uint32_t stride = RendererCommon::getStrideFromFVF(srcDesc.FVF);

	device->SetStreamSource(0, srcBuffer, 0, stride);
	device->SetFVF(srcDesc.FVF);

	HRESULT result = device->ProcessVertices(dwSrcIndex,
		dwDestIndex,
		dwCount,
		destBuffer,
		nullptr,
		0 // must use this flag for clean FFP processing
	);

	return result;
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

	Hook::createHook(kReleaseVertexBufferAddr, &hook_ReleaseVertexBuffer);
	Hook::createHook(kCreateVertexBufferAddr, &hook_CreateVertexBuffer);
	Hook::createHook(kLockVertexBufferAddr, &hook_LockVertexBuffer);
	Hook::createHook(kUnlockVertexBufferAddr, &hook_UnlockVertexBuffer);
	Hook::createHook(kOptimizeVertexBufferAddr, &hook_OptimizeVertexBuffer);
	Hook::createHook(kProcessVertsOnBufferAddr, &hook_ProcessVerticesOnBuffer);

	return result;
}
