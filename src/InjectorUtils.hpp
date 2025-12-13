#pragma once

#include <string>

#include <windows.h>
#include <TlHelp32.h>

namespace InjectorUtils
{
	inline int32_t GetProcessIdByName(const std::wstring& processName)
	{
		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32W);

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (hSnapshot == INVALID_HANDLE_VALUE)
			return 0;

		if (Process32FirstW(hSnapshot, &pe32))
		{
			do
			{
				if (processName == pe32.szExeFile)
				{
					CloseHandle(hSnapshot);
					return pe32.th32ProcessID;
				}
			} while (Process32NextW(hSnapshot, &pe32));
		}

		CloseHandle(hSnapshot);
		return 0;
	}
}