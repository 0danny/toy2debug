#pragma once

#include "Hook.hpp"

class StubHooks : public Hook
{
public:
	StubHooks()
		: Hook("StubHooks") {};

	bool init() override
	{
		// Address definitions
		constexpr int32_t kSoftwareRenderer1Addr = 0x004BCE00;
		constexpr int32_t kFontBuilderAddr = 0x004B4110;

		// Init Hooks
		bool result = true;

		Hook::createHook(kSoftwareRenderer1Addr, &hook_SoftwareRenderer1);
		Hook::createHook(kFontBuilderAddr, &hook_BuildFont);

		return result;
	};

private:
	// Stubs
	static void CON_STDCALL hook_SoftwareRenderer1()
	{
		// Not needed
		return;
	}

	static void* CON_CDECL hook_BuildFont(const char* fontName, int32_t fontSize, const char* charSet)
	{
		// Not needed
		return 0;
	}
};
