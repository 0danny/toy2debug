#include "WindowHooks.hpp"
#include "Settings.hpp"
#include "Mapper.hpp"
#include "ToyTypes.hpp"

#include <print>

LRESULT WINAPI windowWndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_MOVE: {
			if (g_settings.windowStyle != Settings::WindowStyle::Fullscreen)
			{
				CD3DFramework* framework = *Mapper::mapAddress<CD3DFramework**>(0x484008);

				if (framework && framework->hWnd == hWnd)
				{
					GetClientRect(hWnd, &framework->rcScreenRect);
					ClientToScreen(hWnd, (LPPOINT)&framework->rcScreenRect);
					ClientToScreen(hWnd, (LPPOINT)&framework->rcScreenRect.right);

					GetClientRect(hWnd, &framework->rcViewportRect);
				}
			}
			break;
		}

		case WM_PAINT: {
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_DESTROY:
		case WM_NCDESTROY: {
			auto* wndData = Mapper::mapAddress<WindowData*>(0x134488);
			wndData->wndIsExiting = 1;
			break;
		}
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int32_t CON_STDCALL hook_BuildWindow()
{
	auto* wndData = Mapper::mapAddress<WindowData*>(0x134488);

	memset(&wndData->wndClass, 0, sizeof(wndData->wndClass));
	wndData->wndClass.cbSize = 48;

	CoInitialize(0);

	constexpr char kClassName[] = "Toy2Debug - Toy Story 2: Buzz Lightyear to the Rescue";

	wndData->wndClass.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
	wndData->wndClass.lpfnWndProc = windowWndProc;
	wndData->wndClass.cbClsExtra = 0;
	wndData->wndClass.cbWndExtra = 0;
	wndData->wndClass.hInstance = wndData->hInstance;
	wndData->wndClass.hIcon = LoadIconA(wndData->hInstance, "AppIcon");
	wndData->wndClass.lpszMenuName = "AppMenu";
	wndData->wndClass.lpszClassName = kClassName;
	wndData->wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

	if (! RegisterClassExA(&wndData->wndClass))
	{
		constexpr DWORD formatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM;
		char* buffer = nullptr;

		FormatMessageA(formatFlags, 0, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 0, 0);
		MessageBoxA(0, buffer, "GetLastError", MB_ICONINFORMATION | MB_OK);
		LocalFree(buffer);

		return FALSE;
	}

	DWORD windowStyle;
	DWORD exStyle;

	bool isFullscreen = (g_settings.windowStyle == Settings::WindowStyle::Fullscreen);
	bool isBorderless = (g_settings.windowStyle == Settings::WindowStyle::Borderless);

	switch (g_settings.windowStyle)
	{
		case Settings::WindowStyle::Fullscreen:
			windowStyle = WS_POPUP | WS_VISIBLE;
			exStyle = WS_EX_TOPMOST;
			break;

		case Settings::WindowStyle::Windowed:
			windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
			exStyle = WS_EX_APPWINDOW;
			break;

		case Settings::WindowStyle::Borderless:
			windowStyle = WS_POPUP | WS_VISIBLE;
			exStyle = WS_EX_APPWINDOW;
			break;
	}

	RECT windowRect = { 0, 0, g_settings.width, g_settings.height };

	if (! isFullscreen && ! isBorderless)
		AdjustWindowRectEx(&windowRect, windowStyle, FALSE, exStyle);

	int32_t windowWidth = windowRect.right - windowRect.left;
	int32_t windowHeight = windowRect.bottom - windowRect.top;

	// Center window on screen
	int32_t screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int32_t screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int32_t posX = (isFullscreen || isBorderless) ? 0 : (screenWidth - windowWidth) / 2;
	int32_t posY = (isFullscreen || isBorderless) ? 0 : (screenHeight - windowHeight) / 2;

	HWND window = CreateWindowExA(exStyle, kClassName, kClassName, windowStyle, posX, posY, windowWidth, windowHeight, 0, 0, wndData->hInstance, 0);

	wndData->mainHwnd = window;

	if (! window)
	{
		std::println("Failed to create main window!");
		return FALSE;
	}

	UpdateWindow(window);

	SetForegroundWindow(window);
	SetFocus(window);
	SetActiveWindow(window);

	wndData->hAccTable = LoadAcceleratorsA(wndData->hInstance, "AppAccel");

	static uint32_t g_sysParamsInfo = 0;
	if (SystemParametersInfoA(SPI_SCREENSAVERRUNNING, TRUE, &g_sysParamsInfo, 0))
	{
		atexit([] {
			// being faithful
			SystemParametersInfoA(SPI_SCREENSAVERRUNNING, g_sysParamsInfo, 0, 0);
		});
	}

	ShowCursor(TRUE);
	ShowWindow(window, SW_SHOW);

	return TRUE;
}

bool WindowHooks::init()
{
	// Address definitions
	const int32_t kBuildWindow = Mapper::mapAddress(0xA6B30);

	// Init Hooks
	Hook::createHook(kBuildWindow, &hook_BuildWindow);

	return ! Hook::hasFailedHooks();
}
