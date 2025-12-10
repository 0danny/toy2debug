#pragma once

#include <cstdio>
#include <print>
#include <windows.h>

namespace Utils
{
	void initConsole()
	{
		AllocConsole();
		FILE* f;

		freopen_s(&f, "CONOUT$", "w", stdout);
	}
}
