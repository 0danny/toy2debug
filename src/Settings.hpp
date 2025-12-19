#pragma once

#include <cstdint>

namespace Settings
{
	enum RenderAPI : int32_t
	{
		DirectX6,
		DirectX9
	};

	enum WindowStyle : int32_t
	{
		Fullscreen,
		Windowed,
		Borderless
	};

	struct LoaderSettings
	{
		RenderAPI renderApi = DirectX6;
		WindowStyle windowStyle = Windowed;
		char gamePath[256] = { 0 };
		int32_t width = 1600;
		int32_t height = 900;
		bool use32BitColors = true;

		bool correctFramerate = true;
		bool skipCopyrightESRB = false;
		bool disableMovies = false;
		bool widescreenSupport = true;
		bool bigRenderDistance = true;
		bool bypassRegistryKeys = false;
	};

	bool save();
	bool load();
}

inline Settings::LoaderSettings g_settings = {};
