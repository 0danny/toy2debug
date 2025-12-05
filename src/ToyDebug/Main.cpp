#include <windows.h>
#include <print>

#include "Renderer/Renderer.hpp"
#include "Utils.hpp"

namespace
{
	// Renderer
	inline Renderer g_renderer;
}

void initRenderer()
{
	g_renderer.init();
}

DWORD WINAPI mainThread(LPVOID)
{
	Utils::initConsole();

	// allow module to finish initialisation

	initRenderer();

	std::println("[Main]: Installing rendering hooks.");

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if ( reason == DLL_PROCESS_ATTACH )
	{
		DisableThreadLibraryCalls(hModule);
		CreateThread(nullptr, 0, mainThread, nullptr, 0, nullptr);
	}

	return TRUE;
}
