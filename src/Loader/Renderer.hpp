#pragma once

#include <cstdint>

#include <windows.h>

class Renderer final
{
public:
	explicit Renderer(HINSTANCE hInstance)
		: m_hInstance(hInstance) {};

	int32_t init();
	void run();
	void cleanup();

private:
	bool createDeviceD3D();

	static LRESULT CALLBACK wndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam);

private:
	HWND m_hWnd;
	HINSTANCE m_hInstance;
};
