#pragma once

#include <string>

namespace Injector
{
	bool Inject(const std::wstring& processName, const std::string& dllPath);
	bool Inject(int32_t processId, const std::string& dllPath);
}
