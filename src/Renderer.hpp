#pragma once

#include "Tabs/Tab.hpp"

#include <cstdint>
#include <vector>
#include <windows.h>

class Renderer final
{
public:
	explicit Renderer(HINSTANCE hInstance);

	int32_t init();
	void run();
	void cleanup();

private:
	bool createDeviceD3D();
	void drawTabs();
	void setTheme();

	static LRESULT CALLBACK wndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam);

private:
	std::vector<Tab::SharedPtr> m_tabs;

	HWND m_hWnd;
	HINSTANCE m_hInstance;
};
