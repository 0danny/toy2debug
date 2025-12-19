#include "Renderer.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_wndproc_wrapper.h>

#include <d3d9.h>
#include <string_view>

#include "Tabs/MainTab.hpp"
#include "Tabs/DebugTab.hpp"

#include "Settings.hpp"
#include <cmath>

namespace
{
	constexpr static std::string_view kAppTitle = "Toy2Debug Loader";
	constexpr static int32_t kWindowWidth = 600;
	constexpr static int32_t kWindowHeight = 500;
	constexpr static int32_t kWindowX = 100;
	constexpr static int32_t kWindowY = 100;

	static LPDIRECT3D9 g_pD3D = nullptr;
	static LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;
	static D3DPRESENT_PARAMETERS g_d3dpp = {};

	void cleanupD3DDevice()
	{
		if (g_pd3dDevice)
		{
			g_pd3dDevice->Release();
			g_pd3dDevice = nullptr;
		}

		if (g_pD3D)
		{
			g_pD3D->Release();
			g_pD3D = nullptr;
		}
	}
}

Renderer::Renderer(HINSTANCE hInstance)
	: m_hInstance(hInstance)
{
	m_tabs = {
		std::make_shared<MainTab>(),
		std::make_shared<DebugTab>(),
	};

	for (auto& tab : m_tabs)
		tab->init();
}

int32_t Renderer::init()
{
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, wndProc, 0L, 0L, m_hInstance, NULL, NULL, NULL, NULL, kAppTitle.data(), NULL };
	RegisterClassEx(&wc);

	m_hWnd = CreateWindow(kAppTitle.data(), kAppTitle.data(), WS_OVERLAPPEDWINDOW, kWindowX, kWindowY, kWindowWidth, kWindowHeight, NULL, NULL, wc.hInstance, NULL);

	if (! createDeviceD3D())
	{
		cleanupD3DDevice();
		UnregisterClass(kAppTitle.data(), wc.hInstance);
		return 0;
	}

	HICON iconBig = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(101));
	HICON iconSmall = iconBig;

	SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)iconBig);
	SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)iconSmall);

	ShowWindow(m_hWnd, SW_SHOWDEFAULT);
	UpdateWindow(m_hWnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 1.0f;
	io.FontDefault = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.0f);

	ImFontConfig config;
	config.OversampleH = 3;
	config.OversampleV = 3;

	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX9_Init(g_pd3dDevice);

	setTheme();

	return 1;
}

bool Renderer::createDeviceD3D()
{
	g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (! g_pD3D)
		return false;

	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	g_d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
	g_d3dpp.MultiSampleQuality = 0;

	return g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) >= 0;
}

void Renderer::drawTabs()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);

	// clang-format off
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar 
		| ImGuiWindowFlags_NoCollapse 
		| ImGuiWindowFlags_NoResize 
		| ImGuiWindowFlags_NoMove 
		| ImGuiWindowFlags_NoBringToFrontOnFocus 
		| ImGuiWindowFlags_NoNavFocus;
	// clang-format on

	ImGui::Begin("MainWindow", nullptr, windowFlags);

	if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None))
	{
		for (auto& tab : m_tabs)
		{
			if (ImGui::BeginTabItem(tab->getName().c_str()))
			{
				tab->render();
				ImGui::EndTabItem();
			}
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void Renderer::run()
{
	MSG msg {};

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		drawTabs();

		ImGui::EndFrame();

		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(40, 40, 40), 1.0f, 0);

		if (g_pd3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			g_pd3dDevice->EndScene();
		}

		g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
	}
}

void Renderer::cleanup()
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	cleanupD3DDevice();
	DestroyWindow(m_hWnd);
	UnregisterClass(kAppTitle.data(), m_hInstance);
}

LRESULT Renderer::wndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
		case WM_SIZE:
			if (! g_pd3dDevice || wParam == SIZE_MINIMIZED)
				return 0;

			ImGui_ImplDX9_InvalidateDeviceObjects();

			g_d3dpp.BackBufferWidth = LOWORD(lParam);
			g_d3dpp.BackBufferHeight = HIWORD(lParam);

			if (g_pd3dDevice->Reset(&g_d3dpp) == D3DERR_INVALIDCALL)
				return 0;

			ImGui_ImplDX9_CreateDeviceObjects();
			return 0;
		case WM_DESTROY:
			Settings::save();
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Renderer::setTheme()
{
	// Catppuccin Mocha Theme < brenocq
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// Catppuccin Mocha Palette
	// --------------------------------------------------------
	const ImVec4 base = ImVec4(0.117f, 0.117f, 0.172f, 1.0f); // #1e1e2e
	const ImVec4 mantle = ImVec4(0.109f, 0.109f, 0.156f, 1.0f); // #181825
	const ImVec4 surface0 = ImVec4(0.200f, 0.207f, 0.286f, 1.0f); // #313244
	const ImVec4 surface1 = ImVec4(0.247f, 0.254f, 0.337f, 1.0f); // #3f4056
	const ImVec4 surface2 = ImVec4(0.290f, 0.301f, 0.388f, 1.0f); // #4a4d63
	const ImVec4 overlay0 = ImVec4(0.396f, 0.403f, 0.486f, 1.0f); // #65677c
	const ImVec4 overlay2 = ImVec4(0.576f, 0.584f, 0.654f, 1.0f); // #9399b2
	const ImVec4 text = ImVec4(0.803f, 0.815f, 0.878f, 1.0f); // #cdd6f4
	const ImVec4 subtext0 = ImVec4(0.639f, 0.658f, 0.764f, 1.0f); // #a3a8c3
	const ImVec4 mauve = ImVec4(0.796f, 0.698f, 0.972f, 1.0f); // #cba6f7
	const ImVec4 peach = ImVec4(0.980f, 0.709f, 0.572f, 1.0f); // #fab387
	const ImVec4 yellow = ImVec4(0.980f, 0.913f, 0.596f, 1.0f); // #f9e2af
	const ImVec4 green = ImVec4(0.650f, 0.890f, 0.631f, 1.0f); // #a6e3a1
	const ImVec4 teal = ImVec4(0.580f, 0.886f, 0.819f, 1.0f); // #94e2d5
	const ImVec4 sapphire = ImVec4(0.458f, 0.784f, 0.878f, 1.0f); // #74c7ec
	const ImVec4 blue = ImVec4(0.533f, 0.698f, 0.976f, 1.0f); // #89b4fa
	const ImVec4 lavender = ImVec4(0.709f, 0.764f, 0.980f, 1.0f); // #b4befe

	// Main window and backgrounds
	colors[ImGuiCol_WindowBg] = base;
	colors[ImGuiCol_ChildBg] = base;
	colors[ImGuiCol_PopupBg] = surface0;
	colors[ImGuiCol_Border] = surface1;
	colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_FrameBg] = surface0;
	colors[ImGuiCol_FrameBgHovered] = surface1;
	colors[ImGuiCol_FrameBgActive] = surface2;
	colors[ImGuiCol_TitleBg] = mantle;
	colors[ImGuiCol_TitleBgActive] = surface0;
	colors[ImGuiCol_TitleBgCollapsed] = mantle;
	colors[ImGuiCol_MenuBarBg] = mantle;
	colors[ImGuiCol_ScrollbarBg] = surface0;
	colors[ImGuiCol_ScrollbarGrab] = surface2;
	colors[ImGuiCol_ScrollbarGrabHovered] = overlay0;
	colors[ImGuiCol_ScrollbarGrabActive] = overlay2;
	colors[ImGuiCol_CheckMark] = green;
	colors[ImGuiCol_SliderGrab] = sapphire;
	colors[ImGuiCol_SliderGrabActive] = blue;
	colors[ImGuiCol_Button] = surface0;
	colors[ImGuiCol_ButtonHovered] = surface1;
	colors[ImGuiCol_ButtonActive] = surface2;
	colors[ImGuiCol_Header] = surface0;
	colors[ImGuiCol_HeaderHovered] = surface1;
	colors[ImGuiCol_HeaderActive] = surface2;
	colors[ImGuiCol_Separator] = surface1;
	colors[ImGuiCol_SeparatorHovered] = mauve;
	colors[ImGuiCol_SeparatorActive] = mauve;
	colors[ImGuiCol_ResizeGrip] = surface2;
	colors[ImGuiCol_ResizeGripHovered] = mauve;
	colors[ImGuiCol_ResizeGripActive] = mauve;
	colors[ImGuiCol_Tab] = surface0;
	colors[ImGuiCol_TabHovered] = surface2;
	colors[ImGuiCol_TabActive] = surface1;
	colors[ImGuiCol_TabUnfocused] = surface0;
	colors[ImGuiCol_TabUnfocusedActive] = surface1;
	colors[ImGuiCol_PlotLines] = blue;
	colors[ImGuiCol_PlotLinesHovered] = peach;
	colors[ImGuiCol_PlotHistogram] = teal;
	colors[ImGuiCol_PlotHistogramHovered] = green;
	colors[ImGuiCol_TableHeaderBg] = surface0;
	colors[ImGuiCol_TableBorderStrong] = surface1;
	colors[ImGuiCol_TableBorderLight] = surface0;
	colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = surface2;
	colors[ImGuiCol_DragDropTarget] = yellow;
	colors[ImGuiCol_NavHighlight] = lavender;
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
	colors[ImGuiCol_Text] = text;
	colors[ImGuiCol_TextDisabled] = subtext0;

	// Rounded corners
	style.WindowRounding = 6.0f;
	style.ChildRounding = 6.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;

	// Padding and spacing
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.FramePadding = ImVec2(5.0f, 3.0f);
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.IndentSpacing = 21.0f;
	style.ScrollbarSize = 14.0f;
	style.GrabMinSize = 10.0f;

	// Borders
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.TabBorderSize = 0.0f;
}
