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

namespace
{
	constexpr static std::string_view kAppTitle = "Toy2Debug Loader";
	constexpr static int32_t kWindowWidth = 600;
	constexpr static int32_t kWindowHeight = 600;
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
