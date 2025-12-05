#include "LightsHooks.hpp"
#include "Renderer/RendererCommon.hpp"

#include <print>

#include <d3dtypes.h>

namespace
{
	struct NuLight9
	{
		D3DLIGHT9 light;
		int32_t index; // -1 means not assigned yet
	};

	static uint32_t g_nextLightIndex = 0;
}

HRESULT CON_CDECL hook_SetLightState(D3DLIGHTSTATETYPE lightState, DWORD value)
{
	if ( ! RendererCommon::g_framework || ! RendererCommon::g_framework->pd3dDevice )
		return E_FAIL;

	auto* device = RendererCommon::g_framework->pd3dDevice;

	switch ( lightState )
	{
		case D3DLIGHTSTATE_AMBIENT:
			// Global ambient colour
			return device->SetRenderState(D3DRS_AMBIENT, value);

		case D3DLIGHTSTATE_COLORVERTEX:
			// Use vertex colours in lighting
			return device->SetRenderState(D3DRS_COLORVERTEX, value ? TRUE : FALSE);

		case D3DLIGHTSTATE_MATERIAL:
		{
			if ( value == 0 )
			{
				device->SetRenderState(D3DRS_LIGHTING, FALSE);
				return D3D_OK;
			}

			auto* mat = reinterpret_cast<D3DMATERIAL9*>(value);

			// Make sure lighting is on when they set a material
			device->SetRenderState(D3DRS_LIGHTING, TRUE);
			return device->SetMaterial(mat);
		}

		default:
			// Unknown or unused state - ignore safely
			return D3D_OK;
	}
}

HRESULT CON_CDECL hook_CreateLight(NuLight9** lpNuLight)
{
	if ( ! lpNuLight )
		return E_INVALIDARG;

	NuLight9* light = new NuLight9;
	ZeroMemory(&light->light, sizeof(D3DLIGHT9));

	// Reasonable defaults
	light->light.Type = D3DLIGHT_DIRECTIONAL;
	light->light.Range = 1000.0f;
	light->index = -1;

	*lpNuLight = light;

	return D3D_OK;
}

HRESULT CON_CDECL hook_SetLight(NuLight9* nuLight, D3DLIGHT* d3dLight)
{
	if ( ! nuLight || ! d3dLight )
		return E_INVALIDARG;

	ZeroMemory(&nuLight->light, sizeof(D3DLIGHT9));

	nuLight->light.Type = d3dLight->dltType;

	// Old struct only has one colour, so use it for all three
	nuLight->light.Diffuse = d3dLight->dcvColor;
	nuLight->light.Specular = d3dLight->dcvColor;
	nuLight->light.Ambient = d3dLight->dcvColor;

	nuLight->light.Position = d3dLight->dvPosition;
	nuLight->light.Direction = d3dLight->dvDirection;
	nuLight->light.Range = d3dLight->dvRange;
	nuLight->light.Falloff = d3dLight->dvFalloff;
	nuLight->light.Attenuation0 = d3dLight->dvAttenuation0;
	nuLight->light.Attenuation1 = d3dLight->dvAttenuation1;
	nuLight->light.Attenuation2 = d3dLight->dvAttenuation2;
	nuLight->light.Theta = d3dLight->dvTheta;
	nuLight->light.Phi = d3dLight->dvPhi;

	return D3D_OK;
}

HRESULT CON_CDECL hook_AddLight(NuLight9* nuLight)
{
	if ( ! nuLight || ! RendererCommon::g_framework || ! RendererCommon::g_framework->pd3dDevice )
		return E_FAIL;

	auto* device = RendererCommon::g_framework->pd3dDevice;

	// Assign an index if we do not have one yet
	if ( nuLight->index < 0 )
	{
		D3DCAPS9 caps;

		uint32_t maxLights = 8;

		if ( SUCCEEDED(device->GetDeviceCaps(&caps)) )
			maxLights = caps.MaxActiveLights;

		nuLight->index = static_cast<int32_t>(g_nextLightIndex++ % maxLights);
	}

	HRESULT hr = device->SetLight(nuLight->index, &nuLight->light);

	if ( FAILED(hr) )
		return hr;

	hr = device->LightEnable(nuLight->index, TRUE);

	return hr;
}

HRESULT CON_CDECL hook_DeleteLight(NuLight9* nuLight)
{
	// TODO: Fix
	return D3D_OK;
}

HRESULT CON_CDECL hook_ReleaseLight(NuLight9* nuLight)
{
	// TODO: Fix
	return D3D_OK;
}

bool LightsHooks::init()
{
	// Address definitions
	constexpr int32_t kInitializeForWindowAddr = 0x004AEDA0;
	constexpr int32_t kCreateLightAddr = 0x004ABF30;
	constexpr int32_t kSetLightAddr = 0x004ABF50;
	constexpr int32_t kAddLightAddr = 0x004ABF60;
	constexpr int32_t kSetLightStateAddr = 0x004ABFA0;
	constexpr int32_t kDeleteLightAddr = 0x004ABF80;
	constexpr int32_t kReleaseLightAddr = 0x004ABFC0;

	// Init Hooks
	bool result = false;

	result = Hook::createHook(kSetLightStateAddr, &hook_SetLightState);
	result = Hook::createHook(kCreateLightAddr, &hook_CreateLight);
	result = Hook::createHook(kSetLightAddr, &hook_SetLight);
	result = Hook::createHook(kAddLightAddr, &hook_AddLight);
	result = Hook::createHook(kDeleteLightAddr, &hook_DeleteLight);
	result = Hook::createHook(kReleaseLightAddr, &hook_ReleaseLight);

	return result;
}
