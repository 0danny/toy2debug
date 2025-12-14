#include "DirectX6.hpp"

#include "Hooks/DX6/DeviceHooks.hpp"

#include <print>

bool DirectX6::init()
{
	m_hooks = {
		std::make_shared<DX6::DeviceHooks>(),
	};

	bool init = true;

	for (auto& hook : m_hooks)
	{
		if (! hook->init())
		{
			std::println("[DirectX6]: Hook {} failed.", hook->getName());
			init = false;
		}
	}

	patchRenderTarget();

	return init;
}

void DirectX6::patchRenderTarget()
{
	/*
	Allow resolutions above 1080p on DirectX6, there is a check inside of DIRECT3DDEVICEI::Init
	which returns INVALID_PARAMETER if the render target width or height is > 2048.
	This method nops that entire statement out.

	.text:74005470                 mov     eax, 800h
	.text:74005475                 cmp     [ebp+var_64], eax
	.text:74005478                 ja      loc_74005847
	.text:7400547E                 cmp     [ebp+var_68], eax
	.text:74005481                 ja      loc_74005847
	*/

	HMODULE modBase = LoadLibraryA("d3dim.dll");
	if (! modBase)
	{
		std::println("[patchRenderTarget]: d3dim.dll handle is bad");
		return;
	}

	uint8_t* patchAddr = reinterpret_cast<uint8_t*>(modBase) + 0x5470;

	DWORD size = 23;
	DWORD oldProtect = 0;

	if (! VirtualProtect(patchAddr, size, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		std::println("[patchRenderTarget]: VirtualProtect failed");
		return;
	}

	for (int32_t byte = 0; byte < size; byte++)
		patchAddr[byte] = 0x90;

	VirtualProtect(patchAddr, size, oldProtect, &oldProtect);
}
