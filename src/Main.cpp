#include "Renderer.hpp"
#include "Utils.hpp"
#include "Settings.hpp"

#include <windows.h>

int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int32_t nCmdShow)
{
	Utils::initConsole();
	Utils::setConsoleTitle("Toy2Debug");

	Settings::load();

	Renderer renderer(hInstance);

	int32_t result = renderer.init();

	if (result)
		renderer.run(); // blocking

	return result;
}
