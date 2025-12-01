#pragma once

#include <windows.h>
#include <cstdio>
#include <print>

namespace Utils
{
	void InitConsole()
	{
		AllocConsole();
		FILE* f;

		freopen_s(&f, "CONOUT$", "w", stdout);
	}
}
