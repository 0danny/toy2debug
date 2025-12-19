#include "MainTab.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

// Hooks
#include "RenderAPI/DirectX6.hpp"
#include "RenderAPI/DirectX9.hpp"
#include "Hooks/CommonHooks.hpp"
#include "Hooks/WindowHooks.hpp"
#include "Hooks/GameVariables.hpp"

#include <fstream>
#include <print>

#include <ImGui/imgui.h>

/*
	Must have (no checkbox):
		Remove resolution selection screen - Done
		Fix the unable to enumerate device - Done
		Remove registry check + validate.tta - Done
		32-bit resolution selection - Done

	Optionals:
		Fix FPS issues - Done
		Skip ESRB & Copyright screens - Done
		Widescreen + FOV fix - Done
		Render distance increase - Done
		Various map fixes (Al's toy barn)
*/

// I use this for debugging, if I need to set
// software breakpoints before the game boots
constexpr bool showMapButton = false;

void MainTab::init() {}

void MainTab::render()
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
	const char* titleText = "Toy2Debug v0.1";
	ImGui::SetWindowFontScale(1.5f);

	float windowWidth = ImGui::GetContentRegionAvail().x;
	float titleWidth = ImGui::CalcTextSize(titleText).x;

	float titleX = (windowWidth - titleWidth) * 0.5f;
	ImGui::SetCursorPosX(titleX);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(titleText);

	ImGui::SameLine();
	ImGui::SetWindowFontScale(1.0f);

	ImGui::TextUnformatted("By Danny <3");
	ImGui::PopStyleColor();

	// Game Patches
	ImGui::SeparatorText("Game Patches");

	ImGui::BeginTable("patch_table", 3, ImGuiTableFlags_SizingStretchSame);

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Checkbox("Correct Framerate", &g_settings.correctFramerate);

	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("Skip ESRB & Copyright", &g_settings.skipCopyrightESRB);

	ImGui::TableSetColumnIndex(2);
	ImGui::Checkbox("Disable All Movies", &g_settings.disableMovies);

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Checkbox("Widescreen Support", &g_settings.widescreenSupport);

	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("Big Render Distance", &g_settings.bigRenderDistance);

	ImGui::TableSetColumnIndex(2);
	ImGui::Checkbox("Bypass Registry Keys", &g_settings.bypassRegistryKeys);

	ImGui::EndTable();

	ImGui::Spacing();
	ImGui::Spacing();

	// Launch Settings
	ImGui::SeparatorText("Launch Settings");

	ImGui::PushItemWidth(-1);
	ImGui::InputText("##game_path", g_settings.gamePath, sizeof(g_settings.gamePath), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();

	if (ImGui::Button("Change Path", { ImGui::GetContentRegionAvail().x, 30 }))
	{
		std::string selectedPath;

		if (Utils::selectFile(selectedPath))
		{
			std::strncpy(g_settings.gamePath, selectedPath.c_str(), sizeof(g_settings.gamePath) - 1);
			std::println("[MainTab]: Setting new game path to - {}", selectedPath);

			Settings::save();
		}
	}

	ImGui::BeginTable("launch_options", 4, ImGuiTableFlags_SizingFixedFit);

	ImGui::TableSetupColumn("Width Label", ImGuiTableColumnFlags_WidthFixed, 50.0f);
	ImGui::TableSetupColumn("Width Input", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Height Label", ImGuiTableColumnFlags_WidthFixed, 50.0f);
	ImGui::TableSetupColumn("Height Input", ImGuiTableColumnFlags_WidthStretch);

	ImGui::TableNextRow();

	// Width Label
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("Width");

	// Width Input
	ImGui::TableSetColumnIndex(1);
	ImGui::PushItemWidth(-1);
	ImGui::InputInt("##width", &g_settings.width);
	ImGui::PopItemWidth();

	// Height Label
	ImGui::TableSetColumnIndex(2);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("Height");

	// Height Input
	ImGui::TableSetColumnIndex(3);
	ImGui::PushItemWidth(-1);
	ImGui::InputInt("##height", &g_settings.height);
	ImGui::PopItemWidth();

	ImGui::EndTable();

	ImGui::Checkbox("32 bit Colour", &g_settings.use32BitColors);
	ImGui::SameLine();

	auto windowStyleInt = static_cast<int32_t>(g_settings.windowStyle);

	if (ImGui::RadioButton("Fullscreen", &windowStyleInt, 0))
		g_settings.windowStyle = Settings::WindowStyle::Fullscreen;

	ImGui::SameLine();

	if (ImGui::RadioButton("Windowed", &windowStyleInt, 1))
		g_settings.windowStyle = Settings::WindowStyle::Windowed;

	ImGui::SameLine();

	if (ImGui::RadioButton("Borderless", &windowStyleInt, 2))
		g_settings.windowStyle = Settings::WindowStyle::Borderless;

	ImGui::Spacing();

	if (showMapButton)
	{
		if (ImGui::Button("Map Game", { ImGui::GetContentRegionAvail().x, 30 }))
			mapGame();
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);

	if (ImGui::Button("Launch Game", { ImGui::GetContentRegionAvail().x, 30 }))
	{
		Settings::save();

		if (! showMapButton)
			mapGame();

		m_mapper.runGame();
	}

	ImGui::PopStyleColor();
}

void MainTab::mapGame()
{
	m_mapper.map(g_settings.gamePath);

	std::vector<Hook::SharedPtr> hooks = {
		std::make_shared<CommonHooks>(),
		std::make_shared<WindowHooks>(),
	};

	GameVariables::init();

	switch (g_settings.renderApi)
	{
		case Settings::RenderAPI::DirectX6:
			hooks.push_back(std::make_shared<DirectX6>());
			break;
		case Settings::RenderAPI::DirectX9:
			hooks.push_back(std::make_shared<DirectX9>());
			break;
	}

	if (MH_Initialize())
	{
		std::println("[MainTab]: Could not init MinHook.");
		return;
	}

	for (auto& hook : hooks)
	{
		bool result = hook->init();

		if (result)
			std::println("[MainTab]: Initalised [{}] hook", hook->getName());
		else
			std::println("[MainTab]: Failed to initialise [{}] hook", hook->getName());
	}

	if (MH_EnableHook(MH_ALL_HOOKS))
		std::println("[MainTab]: Could not enable all hooks.");
}
