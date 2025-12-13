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
		int32_t width = 1920;
		int32_t height = 1080;
		bool fullscreen = false;
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
