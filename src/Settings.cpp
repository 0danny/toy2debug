#include "Settings.hpp"
#include "json.hpp"

#include <fstream>
#include <cstring>
#include <print>
#include <string_view>
#include <charconv>
#include <stdexcept>
#include <filesystem>
#include <windows.h>

using json = nlohmann::json;

namespace Settings
{
	constexpr std::string_view kSettingsFile = "settings.json";

	StartupOptions parseArguments(int argc, const char* const* argv)
	{
		StartupOptions options;

		for (int i = 1; i < argc; ++i)
		{
			std::string_view name = argv[i];

			if (name == "--auto-start")
				options.autoStart = true;
			else if (name == "--hidden")
				options.autoStart = options.hidden = true;
			else
			{
				if (++i == argc || std::string_view(argv[i]).starts_with("--"))
					throw std::invalid_argument("Missing value for " + std::string(name));

				std::string_view value = argv[i];

				auto invalid = [&] { throw std::invalid_argument("Invalid value for " + std::string(name) + ": " + std::string(value)); };

				auto boolean = [&] {
					if (value != "true" && value != "false")
						invalid();

					return value == "true";
				};

				if (name == "--game-path")
				{
					auto path = std::filesystem::absolute(value).u8string();

					if (value.empty() || path.size() >= sizeof(g_settings.gamePath))
						invalid();

					std::memcpy(g_settings.gamePath, path.c_str(), path.size() + 1);
				}
				else if (name == "--width" || name == "--height")
				{
					int32_t dimension = 0;

					if (value == "auto")
					{
						DEVMODEA mode {};
						mode.dmSize = sizeof(mode);

						if (! EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &mode))
							throw std::runtime_error("Failed to detect the primary monitor resolution");

						dimension = static_cast<int32_t>(name == "--width" ? mode.dmPelsWidth : mode.dmPelsHeight);
					}
					else
					{
						auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), dimension);

						if (error != std::errc() || end != value.data() + value.size())
							invalid();
					}

					if (dimension <= 0)
						invalid();

					(name == "--width" ? g_settings.width : g_settings.height) = dimension;
				}
				else if (name == "--window-style")
				{
					if (value != "fullscreen" && value != "windowed" && value != "borderless")
						invalid();

					g_settings.windowStyle = value == "fullscreen" ? Fullscreen : value == "windowed" ? Windowed : Borderless;
				}
				else if (name == "--use-32-bit-colors")
					g_settings.use32BitColors = boolean();
				else if (name == "--correct-framerate")
					g_settings.correctFramerate = boolean();
				else if (name == "--skip-copyright-esrb")
					g_settings.skipCopyrightESRB = boolean();
				else if (name == "--disable-movies")
					g_settings.disableMovies = boolean();
				else if (name == "--widescreen-support")
					g_settings.widescreenSupport = boolean();
				else if (name == "--big-render-distance")
					g_settings.bigRenderDistance = boolean();
				else if (name == "--bypass-registry-keys")
					g_settings.bypassRegistryKeys = boolean();
				else
					throw std::invalid_argument("Unknown option: " + std::string(name));
			}
		}

		return options;
	}

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

		auto contents = j.dump(4);
		std::ofstream file(kSettingsFile.data(), std::ios::out | std::ios::trunc);
		if (! file.is_open())
		{
			std::println("[Settings]: failed to write settings file.");
			return false;
		}

		file << contents;
		file.close();

		std::println("[Settings]: saved settings file.");

		return true;
	}

	bool load()
	{
		DEVMODEA mode {};
		mode.dmSize = sizeof(mode);
		if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &mode))
		{
			g_settings.width = static_cast<int32_t>(mode.dmPelsWidth);
			g_settings.height = static_cast<int32_t>(mode.dmPelsHeight);
		}

		std::ifstream file(kSettingsFile.data());

		if (! file.is_open())
		{
			std::println("[Settings]: settings does not exist, using defaults.");
			return true;
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
