#pragma once

#include <windows.h>
#include <cstdio>
#include <print>

namespace Utils
{
	void initConsole()
	{
		AllocConsole();
		FILE* f;

		freopen_s(&f, "CONOUT$", "w", stdout);
	}
}
