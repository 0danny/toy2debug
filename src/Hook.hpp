#pragma once

// Semi abstract class to represent an object that hooks methods
#include <string>
#include <memory>
#include <MinHook.h>

class Hook
{
public:
	using SharedPtr = std::shared_ptr<Hook>;

	Hook(const std::string& hookName)
		: m_hookName(hookName)
		, m_failedHooks(0) {};

	~Hook() = default;

	virtual bool init() = 0;
	std::string getName() const { return m_hookName; };
	bool hasFailedHooks() { return m_failedHooks > 0; }

public:
	// Helper to build an actual minhook hook without the reinterpret casting
	void createHook(int32_t address, void* detour)
	{
		MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(address), detour, NULL);

		if (status != MH_OK)
			m_failedHooks++;
	}

	template <typename T>
	void createHook(int32_t address, void* detour, T* original)
	{
		MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(address), detour, reinterpret_cast<void**>(original));

		if (status != MH_OK)
			m_failedHooks++;
	}

private:
	std::string m_hookName;
	int32_t m_failedHooks;
};

// Some calling convention macros for cleanliness
#define CON_FASTCALL __fastcall
#define CON_STDCALL __stdcall
#define CON_CDECL __cdecl
