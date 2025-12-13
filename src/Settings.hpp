#pragma once

namespace Settings
{
	enum RenderAPI
	{
		DirectX3,
		DirectX9
	};

	struct LoaderSettings
	{
		RenderAPI renderApi = DirectX3;
		char gamePath[512] = { 0 };
		int32_t width = 1600;
		int32_t height = 900;
		bool fullscreen = false;
		bool use32BitColors = true;
	};

	inline bool save()
	{
		// saving of the settings happens here.
	}

	inline bool load()
	{
		// loading of settings happens here.
	}
}

inline Settings::LoaderSettings g_settings = {};
