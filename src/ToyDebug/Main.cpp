#include <windows.h>
#include <print>

#include "Renderer.hpp"
#include "Utils.hpp"

namespace
{
	// Renderer
	inline Renderer g_renderer;
}

void InitRenderer()
{
	g_renderer.Init();
}

DWORD WINAPI MainThread(LPVOID)
{
	Utils::InitConsole();

	// allow module to finish initialisation

	InitRenderer();

	std::println("[Main]: Installing rendering hooks.");

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if ( reason == DLL_PROCESS_ATTACH )
	{
		DisableThreadLibraryCalls(hModule);
		CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
	}

	return TRUE;
}
