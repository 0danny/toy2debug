#include "Settings.hpp"
#include "json.hpp"

#include <fstream>
#include <cstring>
#include <print>
#include <string_view>

using json = nlohmann::json;

namespace Settings
{
	constexpr std::string_view kSettingsFile = "settings.json";

	bool save()
	{
		json j;

		j["renderApi"] = g_settings.renderApi;
		j["gamePath"] = g_settings.gamePath;
		j["width"] = g_settings.width;
		j["height"] = g_settings.height;
		j["windowStyle"] = g_settings.windowStyle;
		j["use32BitColors"] = g_settings.use32BitColors;

		// patches
		j["correctFramerate"] = g_settings.correctFramerate;
		j["skipCopyrightESRB"] = g_settings.skipCopyrightESRB;
		j["disableMovies"] = g_settings.disableMovies;
		j["widescreenSupport"] = g_settings.widescreenSupport;
		j["bigRenderDistance"] = g_settings.bigRenderDistance;
		j["bypassRegistryKeys"] = g_settings.bypassRegistryKeys;

		std::ofstream file(kSettingsFile.data(), std::ios::out | std::ios::trunc);
		if (! file.is_open())
		{
			std::println("[Settings]: failed to write settings file.");
			return false;
		}

		file << j.dump(4);
		file.close();

		std::println("[Settings]: saved settings file.");

		return true;
	}

	bool load()
	{
		std::ifstream file(kSettingsFile.data());

		if (! file.is_open())
		{
			std::println("[Settings]: settings does not exist, creating...");
			return save();
		}

		json j;
		file >> j;
		file.close();

		if (j.contains("renderApi"))
			g_settings.renderApi = j["renderApi"].get<RenderAPI>();

		if (j.contains("gamePath"))
			std::strncpy(g_settings.gamePath, j["gamePath"].get<std::string>().c_str(), sizeof(g_settings.gamePath) - 1);

		if (j.contains("width"))
			g_settings.width = j["width"].get<int32_t>();

		if (j.contains("height"))
			g_settings.height = j["height"].get<int32_t>();

		if (j.contains("windowStyle"))
			g_settings.windowStyle = j["windowStyle"].get<WindowStyle>();

		if (j.contains("use32BitColors"))
			g_settings.use32BitColors = j["use32BitColors"].get<bool>();

		// patches
		if (j.contains("correctFramerate"))
			g_settings.correctFramerate = j["correctFramerate"].get<bool>();

		if (j.contains("skipCopyrightESRB"))
			g_settings.skipCopyrightESRB = j["skipCopyrightESRB"].get<bool>();

		if (j.contains("disableMovies"))
			g_settings.disableMovies = j["disableMovies"].get<bool>();

		if (j.contains("widescreenSupport"))
			g_settings.widescreenSupport = j["widescreenSupport"].get<bool>();

		if (j.contains("bigRenderDistance"))
			g_settings.bigRenderDistance = j["bigRenderDistance"].get<bool>();

		if (j.contains("bypassRegistryKeys"))
			g_settings.bypassRegistryKeys = j["bypassRegistryKeys"].get<bool>();

		std::println("[Settings]: loaded settings file.");

		return true;
	}
}
