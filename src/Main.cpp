#include "Renderer.hpp"

#include <windows.h>

int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int32_t nCmdShow)
{
	auto allocConsole = [] {
		AllocConsole();

		FILE* f;
		freopen_s(&f, "CONOUT$", "w", stdout);
	};

	allocConsole();

	Renderer renderer(hInstance);

	auto result = renderer.init();

	if (result)
		renderer.run(); // blocking

	return result;
}
