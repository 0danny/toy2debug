#pragma once

#include "Mapper.hpp"
#include <print>

namespace GameVariables
{
	inline char* g_cdPath = nullptr;
	inline char* g_dataPath = nullptr;

	inline int32_t* g_registryKeysRead = nullptr;
	inline int32_t* g_isSoftwareRendering = nullptr;
	inline int32_t* g_renderMode = nullptr;
	inline int32_t* g_speedMultiplier = nullptr;

	inline void init()
	{
		std::println("[GameVariables]: Initialising.");

		// init all the game variables
		g_cdPath = Mapper::mapAddress<char*>(0x483144);
		g_dataPath = Mapper::mapAddress<char*>(0x482F40);

		g_registryKeysRead = Mapper::mapAddress<int32_t*>(0x482F3C);
		g_isSoftwareRendering = Mapper::mapAddress<int32_t*>(0x484484);
		g_renderMode = Mapper::mapAddress<int32_t*>(0x134554);
		g_speedMultiplier = Mapper::mapAddress<int32_t*>(0x12F2D4);
	}
}