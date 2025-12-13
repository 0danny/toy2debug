#include "WindowHooks.hpp"
#include "Settings.hpp"
#include "Mapper.hpp"
#include "ToyTypes.hpp"

#include <print>

LRESULT WINAPI windowWndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_GETMINMAXINFO: {
			// Prevent window from being resized in windowed mode
			if (! g_settings.fullscreen)
			{
				MINMAXINFO* mmi = (MINMAXINFO*)lParam;
				RECT windowRect = { 0, 0, g_settings.width, g_settings.height };

				DWORD style = GetWindowLong(hWnd, GWL_STYLE);
				DWORD exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
				AdjustWindowRectEx(&windowRect, style, GetMenu(hWnd) != nullptr, exStyle);

				int windowWidth = windowRect.right - windowRect.left;
				int windowHeight = windowRect.bottom - windowRect.top;

				mmi->ptMinTrackSize.x = windowWidth;
				mmi->ptMinTrackSize.y = windowHeight;
				mmi->ptMaxTrackSize.x = windowWidth;
				mmi->ptMaxTrackSize.y = windowHeight;
			}
			return 0;
		}

		case WM_SETCURSOR: {
			// Always show cursor in windowed mode, regardless of hit test
			if (! g_settings.fullscreen)
			{
				SetCursor(LoadCursor(nullptr, IDC_ARROW));
				ShowCursor(TRUE); // Force cursor visible
				return TRUE;
			}
			break;
		}

		case WM_MOVE: {
			if (! g_settings.fullscreen)
			{
				CD3DFramework* framework = *(CD3DFramework**)MAP_ADDRESS(0x484008);

				if (framework && framework->hWnd == hWnd)
				{
					// Update screen rect to new window position
					GetClientRect(hWnd, &framework->rcScreenRect);
					ClientToScreen(hWnd, (LPPOINT)&framework->rcScreenRect);
					ClientToScreen(hWnd, (LPPOINT)&framework->rcScreenRect.right);

					// Also update viewport rect
					GetClientRect(hWnd, &framework->rcViewportRect);
				}
			}
			break;
		}

		case WM_SIZE: {
			// Don't process size changes during mode changes
			// Just acknowledge it
			return 0;
		}

		case WM_PAINT: {
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_ACTIVATEAPP: {
			// Track if app is active (game might check this)
			// Could set a flag here if needed
			return 0;
		}

		case WM_ACTIVATE: {
			// Handle palette if needed (probably not for modern systems)
			return 0;
		}

		case WM_DESTROY:
		case WM_NCDESTROY: {
			// Mark that window is exiting
			WindowData* wndData = (WindowData*)MAP_ADDRESS(0x134488);
			wndData->wndIsExiting = 1;
			break;
		}

		// Let these through to default handler
		case WM_NCPAINT:
		case WM_NCACTIVATE:
		case WM_MOVING:
			break;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int32_t CON_STDCALL hook_BuildWindow()
{
	WindowData* wndData = (WindowData*)MAP_ADDRESS(0x134488);

	memset(&wndData->wndClass, 0, sizeof(wndData->wndClass));
	wndData->wndClass.cbSize = 48;

	CoInitialize(0);

	constexpr char kClassName[] = "Toy2";

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
		char* buffer;

		FormatMessageA(formatFlags, 0, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 0, 0);
		MessageBoxA(0, buffer, "GetLastError", MB_ICONINFORMATION | MB_OK);
		LocalFree(buffer);

		return FALSE;
	}

	DWORD windowStyle;
	DWORD exStyle;

	if (g_settings.fullscreen)
	{
		windowStyle = WS_POPUP | WS_VISIBLE;
		exStyle = WS_EX_TOPMOST;
	}
	else
	{
		windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
		exStyle = WS_EX_APPWINDOW;
	}

	// Calculate window size including borders for windowed mode
	RECT windowRect = { 0, 0, g_settings.width, g_settings.height };

	if (! g_settings.fullscreen)
		AdjustWindowRectEx(&windowRect, windowStyle, FALSE, exStyle);

	int32_t windowWidth = windowRect.right - windowRect.left;
	int32_t windowHeight = windowRect.bottom - windowRect.top;

	// Center window on screen
	int32_t screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int32_t screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int32_t posX = g_settings.fullscreen ? 0 : (screenWidth - windowWidth) / 2;
	int32_t posY = g_settings.fullscreen ? 0 : (screenHeight - windowHeight) / 2;

	HWND window = CreateWindowExA(exStyle, kClassName, kClassName, windowStyle, posX, posY, windowWidth, windowHeight, 0, 0, wndData->hInstance, 0);

	wndData->mainHwnd = window;

	if (! window)
	{
		std::println("Failed to create main window!");
		return FALSE;
	}

	static uint32_t g_sysParamsInfo = 0;

	UpdateWindow(window);

	SetForegroundWindow(window);
	SetFocus(window);
	SetActiveWindow(window);

	wndData->hAccTable = LoadAcceleratorsA(wndData->hInstance, "AppAccel");

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
	const int32_t kBuildWindow = MAP_ADDRESS(0xA6B30);

	// Init Hooks
	Hook::createHook(kBuildWindow, &hook_BuildWindow);

	return ! Hook::hasFailedHooks();
}
