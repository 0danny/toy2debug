#pragma once

#include <string>
#include <windows.h>

class Mapper final
{
public:
	Mapper()
		: m_startMethod(nullptr) {};

	bool map(const std::string& path);
	void runGame();

	// Translation methods
	template <typename T>
	static T mapAddress(uint32_t address)
	{
		// can recast to any type easily
		return reinterpret_cast<T>((uint32_t)g_newImageBase + address);
	}

	static uint32_t mapAddress(uint32_t address)
	{
		// straight conversion
		return ((uint32_t)g_newImageBase + address);
	}

private:
	static LONG WINAPI vectoredHandler(EXCEPTION_POINTERS* exceptionInfo);

	uint8_t* mapFileToMemory(const std::string& fileName);
	bool fixIAT(uint8_t* modulePtr);
	bool applyReloc(uint32_t newBase, uint32_t oldBase, uint32_t moduleSize);
	bool setupVectorExceptions();

	// Helpers
	IMAGE_NT_HEADERS32* getNtHdrs(uint8_t* buffer);
	IMAGE_DATA_DIRECTORY* getPeDir(uint8_t* buffer, uint32_t dirId);

private:
	typedef void (*startMethod)();

	// arbitrary number I picked, it only matters for me
	// if the module is mapped here, makes debugging easier
	constexpr static int32_t kPreferredBase = 0x20000000;
	static uint8_t* g_newImageBase;
	static int32_t g_imageSize;

	startMethod m_startMethod;
};
