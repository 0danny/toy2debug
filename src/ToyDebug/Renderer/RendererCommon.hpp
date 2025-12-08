#pragma once

#include "ToyTypes.hpp"

namespace RendererCommon
{
	inline CD3DFramework* g_framework = nullptr;

	// This is actually a method in the game (0x004B2B80)
	// so we know for sure these are the only FVF's they will
	// be using.

	/*
	(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1); -> 0x152

	D3DFVF_XYZ = (float, float, float)
	D3DFVF_NORMAL = (float, float, float)
	D3DFVF_DIFFUSE = DWORD
	D3DFVF_TEX1 = (float, float)

	= 36 bytes

	(D3DFVF_XYZRHW | D3DFVF_SPECULAR | D3DFVF_DIFFUSE | D3DFVF_TEX1); -> 0x1C4

	D3DFVF_XYZRHW = (float, float, float, float)
	D3DFVF_SPECULAR = DWORD
	D3DFVF_DIFFUSE = DWORD
	D3DFVF_TEX1 = (float, float)

	= 32 bytes
	*/
	inline uint32_t getStrideFromFVF(uint32_t fvf)
	{
		if ( fvf != 0x112 )
		{
			if ( fvf == 0x152 )
				return 36;

			if ( fvf != 0x1C4 )
				return 0;
		}

		return 32;
	}
}
