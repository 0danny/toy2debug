#include "Renderer.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_wndproc_wrapper.h>

#include <string_view>

#include <d3d9.h>

namespace
{
	static constexpr std::string_view kAppTitle = "Toy2Debug Loader";
	static constexpr int32_t kWindowWidth = 600;
	static constexpr int32_t kWindowHeight = 600;
	static constexpr int32_t kWindowX = 100;
	static constexpr int32_t kWindowY = 100;

	static LPDIRECT3D9 g_pD3D = nullptr;
	static LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;
	static D3DPRESENT_PARAMETERS g_d3dpp = {};

	void cleanupD3DDevice()
	{
		if ( g_pd3dDevice )
		{
			g_pd3dDevice->Release();
			g_pd3dDevice = nullptr;
		}

		if ( g_pD3D )
		{
			g_pD3D->Release();
			g_pD3D = nullptr;
		}
	}
}

int32_t Renderer::init()
{
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, wndProc, 0L, 0L, m_hInstance, NULL, NULL, NULL, NULL, kAppTitle.data(), NULL };
	RegisterClassEx(&wc);

	m_hWnd = CreateWindow(
	    kAppTitle.data(),
	    kAppTitle.data(),
	    WS_POPUP,
	    kWindowX,
	    kWindowY,
	    kWindowWidth,
	    kWindowHeight,
	    NULL,
	    NULL,
	    wc.hInstance,
	    NULL
	);

	if ( ! createDeviceD3D() )
	{
		cleanupD3DDevice();
		UnregisterClass(kAppTitle.data(), wc.hInstance);
		return 0;
	}

	ShowWindow(m_hWnd, SW_SHOWDEFAULT);
	UpdateWindow(m_hWnd);

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	io.FontGlobalScale = 1.25f;
	ImFontConfig config;
	config.OversampleH = 3;
	config.OversampleV = 3;
	io.FontDefault = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.0f);

	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX9_Init(g_pd3dDevice);

	return 0;
}

bool Renderer::createDeviceD3D()
{
	g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if ( ! g_pD3D )
		return false;

	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	g_d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
	g_d3dpp.MultiSampleQuality = 0;

	return g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) >= 0;
}
void Renderer::run()
{
	MSG msg{};
	bool isDragging = false;
	POINT dragOffset = {};

	while ( msg.message != WM_QUIT )
	{
		if ( PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) )
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::Begin("MainWindow", nullptr, window_flags);

		ImVec2 titleBarStart = ImGui::GetCursorScreenPos();
		float windowWidth = ImGui::GetWindowSize().x;
		float titleBarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

		float textWidth = ImGui::CalcTextSize("Toy2Debug Loader").x;
		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::Text("Toy2Debug Loader");

		ImGui::SameLine();
		ImGui::SetCursorPosX(windowWidth - 45.0f);
		if (ImGui::Button("X", ImVec2(35, 25)))
		{
			PostQuitMessage(0);
		}

		ImVec2 titleBarEnd = ImVec2(titleBarStart.x + windowWidth, titleBarStart.y + titleBarHeight);

		ImVec2 mousePos = ImGui::GetMousePos();
		bool isMouseInTitleBar = mousePos.x >= titleBarStart.x && mousePos.x <= titleBarEnd.x &&
		                          mousePos.y >= titleBarStart.y && mousePos.y <= titleBarEnd.y;

		if (isMouseInTitleBar && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mousePos.x < (titleBarEnd.x - 45.0f))
		{
			isDragging = true;
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			RECT windowRect;
			GetWindowRect(m_hWnd, &windowRect);
			dragOffset.x = cursorPos.x - windowRect.left;
			dragOffset.y = cursorPos.y - windowRect.top;
		}

		if (isDragging)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				POINT cursorPos;
				GetCursorPos(&cursorPos);
				SetWindowPos(m_hWnd, NULL, 
					cursorPos.x - dragOffset.x, 
					cursorPos.y - dragOffset.y, 
					0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
			else
			{
				isDragging = false;
			}
		}

		ImGui::Separator();

		if ( ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None) )
		{
			if ( ImGui::BeginTabItem("Tab 1") )
			{
				ImGui::Text("Content for Tab 1");
				ImGui::Button("Button 1");
				ImGui::EndTabItem();
			}

			if ( ImGui::BeginTabItem("Tab 2") )
			{
				ImGui::Text("Content for Tab 2");
				ImGui::Button("Button 2");
				ImGui::EndTabItem();
			}

			if ( ImGui::BeginTabItem("Tab 3") )
			{
				ImGui::Text("Content for Tab 3");
				ImGui::Button("Button 3");
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();

		ImGui::EndFrame();
		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(40, 40, 40), 1.0f, 0);

		if ( g_pd3dDevice->BeginScene() >= 0 )
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
	if ( ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) )
		return true;

	switch ( msg )
	{
		case WM_SIZE:
			if ( g_pd3dDevice && wParam != SIZE_MINIMIZED )
			{
				ImGui_ImplDX9_InvalidateDeviceObjects();

				g_d3dpp.BackBufferWidth = LOWORD(lParam);
				g_d3dpp.BackBufferHeight = HIWORD(lParam);

				HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);

				if ( hr == D3DERR_INVALIDCALL )
					return 0;

				ImGui_ImplDX9_CreateDeviceObjects();
			}
			return 0;
		case WM_DESTROY: PostQuitMessage(0); return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}
