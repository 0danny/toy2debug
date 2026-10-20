#include "Renderer.hpp"
#include "Utils.hpp"
#include "Settings.hpp"
#include "Tabs/MainTab.hpp"

#include <windows.h>
#include <cstdlib>
#include <exception>
#include <algorithm>
#include <string_view>

int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int32_t nCmdShow)
{
	if (! Utils::initConsole(false))
		return 1;

	try
	{
		if (std::find(__argv + 1, __argv + __argc, std::string_view("--help")) != __argv + __argc)
		{
			std::puts(
				"Usage: loader.exe [--auto-start] [--hidden] [options]\n"
				"  --game-path PATH  --width N|auto  --height N|auto\n"
				"  auto uses the primary monitor's current resolution in physical pixels.\n"
				"  --window-style fullscreen|windowed|borderless\n"
				"Boolean options take true or false:\n"
				"  --use-32-bit-colors  --correct-framerate  --skip-copyright-esrb\n"
				"  --disable-movies  --widescreen-support  --big-render-distance  --bypass-registry-keys\n"
				"--hidden implies --auto-start and suppresses the launcher windows.");

			return 0;
		}

		Settings::load();
		auto options = Settings::parseArguments(__argc, __argv);

		if (options.hidden)
			return MainTab().launchGame() ? 0 : 1;

		if (! Utils::initConsole())
			return 1;

		Utils::setConsoleTitle("Toy2Debug");

		Renderer renderer(hInstance);

		if (! renderer.init())
			return 1;

		int result = renderer.run(options.autoStart) ? 0 : 1;
		renderer.cleanup();

		return result;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "Toy2Debug: %s\n", error.what());

		return 1;
	}
}
