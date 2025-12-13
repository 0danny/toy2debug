#pragma once

#include <string>
#include <windows.h>

inline int32_t g_newImageBase = 0;

#define MAP_ADDRESS(address) (g_newImageBase + (int32_t)address)

class Mapper final
{
public:
	bool map(const std::string& path);
	void runMap();

private:
};
