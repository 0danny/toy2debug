#pragma once

#include "Hook.hpp"
#include "Settings.hpp"
#include "GameVariables.hpp"

#include <print>
#include <filesystem>

namespace
{
	typedef FILE*(CON_CDECL* originalFOpen)(const char* fileName, const char* mode);
	inline originalFOpen g_originalFOpen = nullptr;
}

class CommonHooks : public Hook
{
public:
	CommonHooks()
		: Hook("CommonHooks") {};

private:
	static void CON_STDCALL hook_ReadRegistry()
	{
		std::filesystem::path fsPath = g_settings.gamePath;

		std::string basePath = fsPath.parent_path().string() + "//";
		std::string dataPath = (fsPath.parent_path() / "data//").string();

		strcpy(GameVariables::g_cdPath, basePath.c_str());
		strcpy(GameVariables::g_dataPath, dataPath.c_str());

		*GameVariables::g_registryKeysRead = 1;
		*GameVariables::g_isSoftwareRendering = 0;
	}

	static int32_t CON_STDCALL hook_BuildProfileMachine()
	{
		*GameVariables::g_renderMode = 2;
		return TRUE;
	}

	static FILE* CON_CDECL hook_FOpen(const char* fileName, const char* mode)
	{
		// naturally the fopen will now look inside of the loader dir for saves
		// instead of the game dir, this hook fixes that.

		std::filesystem::path fsPath(fileName);
		std::string redirectedPath;

		if (fsPath.extension() == ".sav")
		{
			auto basePath = std::filesystem::path(g_settings.gamePath).parent_path() / fsPath.filename();

			redirectedPath = basePath.string();
			fileName = redirectedPath.c_str();
		}

		return g_originalFOpen(fileName, mode);
	}

	// Framerate fix, original logic from Rib's tool
	static void CON_CDECL hook_FrameTimer(int32_t isGameplayFrame)
	{
		static TIMECAPS timeCaps {};
		static LARGE_INTEGER frequency {};
		static LARGE_INTEGER previousTime, currentTime, elapsedMicroseconds {};
		static int32_t targetFPS = 144;
		static bool timerInit = false;

		if (! timerInit)
		{
			timeGetDevCaps(&timeCaps, sizeof(timeCaps));
			QueryPerformanceFrequency(&frequency);
			timerInit = true;
		}

		const int64_t frameTimeUs = 1000000LL / targetFPS;

		timeBeginPeriod(timeCaps.wPeriodMin);

		if (previousTime.QuadPart == 0)
			QueryPerformanceCounter(&previousTime);

		auto updateElapsedTime = [] {
			QueryPerformanceCounter(&currentTime);
			elapsedMicroseconds.QuadPart = currentTime.QuadPart - previousTime.QuadPart;
			elapsedMicroseconds.QuadPart *= 1000000;
			elapsedMicroseconds.QuadPart /= frequency.QuadPart;
		};

		updateElapsedTime();

		int32_t framerateFactor = (int32_t)(elapsedMicroseconds.QuadPart / frameTimeUs) + 1;

		std::println("Framerate Factor -> {}", framerateFactor);

		*GameVariables::g_speedMultiplier = std::clamp(framerateFactor, 1, 3);

		do
		{
			int32_t sleepTime = (int32_t)((frameTimeUs * framerateFactor - elapsedMicroseconds.QuadPart) / 1000);
			sleepTime = ((sleepTime / timeCaps.wPeriodMin) * timeCaps.wPeriodMin) - timeCaps.wPeriodMin;

			if (sleepTime > 0)
				Sleep(sleepTime);

			updateElapsedTime();

		} while (elapsedMicroseconds.QuadPart < frameTimeUs * framerateFactor);

		QueryPerformanceCounter(&previousTime);
		timeEndPeriod(timeCaps.wPeriodMin);
	}

	void patchCursorHiding()
	{
		// stop the game forcing the cursor to be hidden
		auto* p = Mapper::mapAddress<uint8_t*>(0x12E49);

		for (int32_t byte = 0; byte < 22; byte++)
			p[byte] = 0x90;
	}

	bool init() override
	{
		// Address definitions
		const int32_t kReadRegistry = Mapper::mapAddress(0xA6390);
		const int32_t kBuildProfileMachine = Mapper::mapAddress(0x93A0);
		const int32_t kFOpen = Mapper::mapAddress(0xCF22C);
		const int32_t kFrameTimer = Mapper::mapAddress(0x90860);

		// Init Hooks
		Hook::createHook(kReadRegistry, &hook_ReadRegistry);
		Hook::createHook(kBuildProfileMachine, &hook_BuildProfileMachine);
		Hook::createHook(kFOpen, &hook_FOpen, &g_originalFOpen);
		Hook::createHook(kFrameTimer, &hook_FrameTimer);

		patchCursorHiding();

		return ! Hook::hasFailedHooks();
	}
};
