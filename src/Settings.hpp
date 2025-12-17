#pragma once

#include <cstdint>

namespace Settings
{
	enum RenderAPI
	{
		DirectX6,
		DirectX9
	};

	struct LoaderSettings
	{
		RenderAPI renderApi = DirectX6;
		char gamePath[256] = { 0 };
		int32_t width = 1600;
		int32_t height = 900;
		bool fullscreen = false;
		bool use32BitColors = true;
	};

	bool save();
	bool load();
}

inline Settings::LoaderSettings g_settings = {};
