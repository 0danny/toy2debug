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
		if (g_settings.bypassRegistryKeys)
		{
			std::filesystem::path fsPath = g_settings.gamePath;

			std::string basePath = fsPath.parent_path().string() + "//";
			std::string dataPath = (fsPath.parent_path() / "data//").string();

			strcpy(GameVariables::g_cdPath, basePath.c_str());
			strcpy(GameVariables::g_dataPath, dataPath.c_str());

			*GameVariables::g_registryKeysRead = 1;
			*GameVariables::g_isSoftwareRendering = 0;
			return;
		}

		HKEY hSoftwareKey = nullptr;
		HKEY hGameKey = nullptr;

		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software", 0, KEY_READ, &hSoftwareKey) != ERROR_SUCCESS)
			return;

		if (RegOpenKeyExA(hSoftwareKey, "TravellersTalesToyStory2", 0, KEY_READ, &hGameKey) != ERROR_SUCCESS)
		{
			RegCloseKey(hSoftwareKey);
			return;
		}

		DWORD type = 0;
		DWORD dataSize = 512;
		DWORD dataSizeNoNullT = dataSize - 1;

		if (RegQueryValueExA(hGameKey, "path", nullptr, &type, reinterpret_cast<BYTE*>(GameVariables::g_dataPath), &dataSize) == ERROR_SUCCESS && type == REG_SZ)
			GameVariables::g_dataPath[dataSizeNoNullT] = '\0';
		else
			GameVariables::g_dataPath[0] = '\0';

		type = 0;
		dataSize = 512;

		if (RegQueryValueExA(hGameKey, "cdpath", nullptr, &type, reinterpret_cast<BYTE*>(GameVariables::g_cdPath), &dataSize) == ERROR_SUCCESS && type == REG_SZ)
		{
			GameVariables::g_cdPath[dataSizeNoNullT] = '\0';
		}
		else
		{
			strncpy(GameVariables::g_cdPath, GameVariables::g_dataPath, dataSizeNoNullT);
			GameVariables::g_cdPath[dataSizeNoNullT] = '\0';
		}

		RegCloseKey(hGameKey);
		RegCloseKey(hSoftwareKey);
	}

	static int32_t CON_STDCALL hook_BuildProfileMachine()
	{
		*GameVariables::g_renderMode = 2;
		return TRUE;
	}

	static FILE* CON_CDECL hook_FOpen(const char* fileName, const char* mode)
	{
		// naturally the fopen will now look inside of the loader dir for saves/cfgs
		// instead of the game dir, this hook fixes that.
		std::filesystem::path fsPath(fileName);
		std::string redirectedPath;

		if (fsPath.extension() == ".sav" || fsPath.extension() == ".cfg")
		{
			auto basePath = std::filesystem::path(g_settings.gamePath).parent_path() / fsPath.filename();

			redirectedPath = basePath.string();
			fileName = redirectedPath.c_str();
		}

		// this cd.txt call isn't even used for anything
		// somethings it makes the exception handler shit itself too
		// so lets just not allow it
		if (fsPath.filename() == "cd.txt")
		{
			std::println("[fopen]: Redirecting the cd.txt call.");
			return NULL;
		}

		return g_originalFOpen(fileName, mode);
	}

	// Framerate fix, original logic from Rib's tool
	static void CON_CDECL hook_FrameTimer(int32_t isGameplayFrame)
	{
		static TIMECAPS timeCaps {};
		static LARGE_INTEGER frequency {};
		static LARGE_INTEGER previousTime, currentTime, elapsedMicroseconds {};

		static int32_t targetFPS = 60; // if this goes above 60fps, the game runs at super speed.
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

	static void CON_CDECL hook_SetRenderDistance(float nearDistance, float farDistance)
	{
		*GameVariables::g_nearRenderDistance = INFINITY;
		*GameVariables::g_farRenderDistance = 1000000.0;
		*GameVariables::g_cfgDetail = 2; // render distance fix won't work without max detail setting
	}

	void patchCursorHiding()
	{
		// stop the game forcing the cursor to be hidden
		auto* p = Mapper::mapAddress<uint8_t*>(0x12E49);

		for (int32_t byte = 0; byte < 22; byte++)
			p[byte] = 0x90;
	}

	void patchWidescreenFix()
	{
		// fix 3d stretch
		float aspectRatio = float(g_settings.width) / float(g_settings.height);
		float scaleValue = 1.0f / aspectRatio;

		//  Nu3D::g_primaryCamera->aspectHOverW = 0.75;
		*Mapper::mapAddress<float*>(0xCE092) = scaleValue;
	}

	void patchSkipCopyright()
	{
		uint8_t copyrightPatch[] = {
			0x66,
			0xBF,
			0x01,
			0x00, // mov di, 1
			0x90,
			0x90,
			0x90 // nops
		};

		memcpy(Mapper::mapAddress<void*>(0x38586), copyrightPatch, sizeof(copyrightPatch));
	}

	void patchDisableMovies()
	{
		uint8_t mpegPatch[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

		memcpy(Mapper::mapAddress<void*>(0x8E74C), mpegPatch, sizeof(mpegPatch));

		// disable write to zero aswell
		*GameVariables::g_mpegEnabled = TRUE;
	}

	bool init() override
	{
		// Address definitions
		const int32_t kReadRegistry = Mapper::mapAddress(0xA6390);
		const int32_t kBuildProfileMachine = Mapper::mapAddress(0x93A0);
		const int32_t kFOpen = Mapper::mapAddress(0xCF22C);
		const int32_t kFrameTimer = Mapper::mapAddress(0x90860);
		const int32_t kSetRenderDistance = Mapper::mapAddress(0xBC410);

		// Init Hooks
		Hook::createHook(kReadRegistry, &hook_ReadRegistry);
		Hook::createHook(kBuildProfileMachine, &hook_BuildProfileMachine);
		Hook::createHook(kFOpen, &hook_FOpen, &g_originalFOpen);

		if (g_settings.correctFramerate)
			Hook::createHook(kFrameTimer, &hook_FrameTimer);

		if (g_settings.bigRenderDistance)
			Hook::createHook(kSetRenderDistance, &hook_SetRenderDistance);

		if (g_settings.widescreenSupport)
			patchWidescreenFix();

		if (g_settings.skipCopyrightESRB)
			patchSkipCopyright();

		if (g_settings.disableMovies)
			patchDisableMovies();

		patchCursorHiding();

		return ! Hook::hasFailedHooks();
	}
};
