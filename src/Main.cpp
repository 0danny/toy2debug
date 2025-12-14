#include "Renderer.hpp"
#include "Utils.hpp"

#include <windows.h>

int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int32_t nCmdShow)
{
	Utils::initConsole();

	Renderer renderer(hInstance);

	auto result = renderer.init();

	if (result)
		renderer.run(); // blocking

	return result;
}
