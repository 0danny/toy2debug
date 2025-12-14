#pragma once

#include "Hook.hpp"
#include "Settings.hpp"
#include "Mapper.hpp"

#include <print>
#include <filesystem>

class CommonHooks : public Hook
{
public:
	CommonHooks()
		: Hook("CommonHooks") {};

private:
	static void CON_STDCALL hook_ReadRegistry()
	{
		std::println("Reading registry!");

		std::filesystem::path fsPath = g_settings.gamePath;

		std::string basePath = fsPath.parent_path().string();
		std::string dataPath = (fsPath.parent_path() / "data//").string();

		char* cdPathDst = reinterpret_cast<char*>(MAP_ADDRESS(0x483144));
		char* dataPathDst = reinterpret_cast<char*>(MAP_ADDRESS(0x482F40));

		strcpy(cdPathDst, basePath.c_str());
		strcpy(dataPathDst, dataPath.c_str());

		*(int32_t*)MAP_ADDRESS(0x482F3C) = 1; // g_registryKeysRead
		*(int32_t*)MAP_ADDRESS(0x484484) = 0; // g_isSoftwareRendering
	}

	static void CON_STDCALL hook_ProfileCPU()
	{
		// no-op
		return;
	}

	static int32_t CON_STDCALL hook_BuildProfileMachine()
	{
		// g_renderMode = D3DRenderer
		*(int32_t*)MAP_ADDRESS(0x134554) = 2;

		// useless now that we hook all window/driver code
		return TRUE;
	}

	void patchCursorHiding()
	{
		// remove the show cursor bullshit

		/*
			.text:20012E49 loc_20012E49:
			.text:20012E49                 mov     esi, ds:__imp_ShowCursor
			.text:20012E4F                 push    0               ; bShow
			.text:20012E51                 call    esi ; __imp_ShowCursor
			.text:20012E53                 test    eax, eax
			.text:20012E55                 jl      short loc_20012E5F
			.text:20012E57
			.text:20012E57 loc_20012E57:
			.text:20012E57                 push    0               ; bShow
			.text:20012E59                 call    esi ; __imp_ShowCursor
			.text:20012E5B                 test    eax, eax
			.text:20012E5D                 jge     short loc_20012E57
		*/

		uint8_t* p = (uint8_t*)MAP_ADDRESS(0x12E49);

		for (int32_t byte = 0; byte < 22; byte++)
			p[byte] = 0x90;
	}

	bool init() override
	{
		// Address definitions
		const int32_t kReadRegistry = MAP_ADDRESS(0xA6390);
		const int32_t kProfileCPU = MAP_ADDRESS(0x7EF80);
		const int32_t kBuildProfileMachine = MAP_ADDRESS(0x93A0);

		// Init Hooks
		Hook::createHook(kReadRegistry, &hook_ReadRegistry);
		Hook::createHook(kProfileCPU, &hook_ProfileCPU);
		Hook::createHook(kBuildProfileMachine, &hook_BuildProfileMachine);

		patchCursorHiding();

		return ! Hook::hasFailedHooks();
	}
};
