#include "Injector.hpp"
#include "InjectorUtils.hpp"

#include <print>

namespace Injector
{
	bool Inject(const std::wstring& processName, const std::string& dllPath)
	{
		int32_t processId = InjectorUtils::GetProcessIdByName(processName);

		if (processId == 0)
		{
			std::println("[Injector]: Could not find process, ensure its running.");
			return false;
		}

		std::println("[Injector]: Found process id -> {}.", processId);

		return Inject(processId, dllPath);
	}

	bool Inject(int32_t processId, const std::string& dllPath)
	{
		// Open target process with required permissions
		HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, processId);

		if (! hProcess)
		{
			std::println("[Injector]: Failed to open process. Error: {}", GetLastError());
			return false;
		}

		// Allocate memory in target process for DLL path
		size_t pathSize = dllPath.length() + 1;
		LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		if (! pRemotePath)
		{
			std::println("[Injector]: Failed to allocate memory. Error: {}", GetLastError());
			CloseHandle(hProcess);
			return false;
		}

		// Write DLL path to allocated memory
		if (! WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), pathSize, NULL))
		{
			std::println("[Injector]: Failed to write memory. Error: {}", GetLastError());
			VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		// Get address of LoadLibraryA
		LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

		if (! pLoadLibrary)
		{
			std::println("[Injector]: Failed to get LoadLibraryA address");
			VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		// Create remote thread to load the DLL
		HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemotePath, 0, NULL);

		if (! hThread)
		{
			std::println("[Injector]: Failed to create remote thread. Error: ", GetLastError());
			VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		WaitForSingleObject(hThread, 5000);

		// Cleanup
		VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
		CloseHandle(hThread);
		CloseHandle(hProcess);

		return true;
	}
}
