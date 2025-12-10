#include <cstdint>
#include <print>
#include <filesystem>
#include <windows.h>

#include "Injector.hpp"
#include "Renderer.hpp"
#include "Mapper.hpp"

int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int32_t nCmdShow)
{
	Renderer renderer(hInstance);

	renderer.init();
	renderer.run(); // blocking

	std::println("[Toy2Debug Loader - Danny]");

	// get full path, injector needs absolutes
	auto dllPath = std::filesystem::absolute("toydebug.dll");

	STARTUPINFOA si {};
	PROCESS_INFORMATION pi {};
	si.cb = sizeof(si);

	std::string hardcodedPath = "!";
	std::filesystem::path exePath(hardcodedPath);
	std::string workingDir = exePath.parent_path().string();

	// create suspended
	if (! CreateProcessA(hardcodedPath.c_str(), nullptr, nullptr, nullptr, false, CREATE_SUSPENDED, nullptr, workingDir.c_str(), &si, &pi))
	{
		std::println("[Main]: Failed to start process suspended.");
		return -1;
	}

	std::println("[Main]: Process started suspended.");

	// inject into the suspended process
	if (! Injector::Inject(pi.dwProcessId, dllPath.string()))
	{
		std::println("[Main]: Failed to inject dll.");
		TerminateProcess(pi.hProcess, 0);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return -1;
	}

	std::println("[Main]: Successfully injected dll.");

	// resume the main thread
	ResumeThread(pi.hThread);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return 0;
}
