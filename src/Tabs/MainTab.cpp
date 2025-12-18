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

// Patches I will include
/*
	Must have (no checkbox):
		Remove resolution selection screen
		Fix the unable to enumerate device
		Remove registry check + validate.tta
		32-bit resolution selection

	Optionals:
		Fix FPS issues
		Skip ESRB & Copyright screens
		Widescreen + FOV fix
		Render distance increase
		Various map fixes (Al's toy barn)
*/

void MainTab::init() {}

void MainTab::render()
{
	// Game Patches
	ImGui::SeparatorText("Game Patches (DON'T WORK)");

	static bool placeholder = false;
	ImGui::BeginTable("patch_table", 3, ImGuiTableFlags_SizingStretchSame);

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Checkbox("Correct Framerate", &placeholder);

	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("Skip ESRB & Copyright", &placeholder);

	ImGui::TableSetColumnIndex(2);
	ImGui::Checkbox("Disable Movies", &placeholder);

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Checkbox("Widescreen Support", &placeholder);

	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("Big Render Distance", &placeholder);

	ImGui::EndTable();

	// Launch Settings
	ImGui::SeparatorText("Launch Settings");

	ImGui::PushItemWidth(-1);
	ImGui::InputText("##game_path", g_settings.gamePath, sizeof(g_settings.gamePath), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();

	ImGui::BeginTable("launch_options", 2, ImGuiTableFlags_SizingStretchProp);
	// Width
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::InputInt("Width", &g_settings.width);

	// Height
	ImGui::TableSetColumnIndex(1);
	ImGui::InputInt("Height", &g_settings.height);
	ImGui::EndTable();

	ImGui::Checkbox("32 bit Colour", &g_settings.use32BitColors);
	ImGui::SameLine();
	ImGui::Checkbox("Fullscreen", &g_settings.fullscreen);

	ImGui::Spacing();

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

	if (ImGui::Button("Map Game", { ImGui::GetContentRegionAvail().x, 30 }))
		mapGame();

	if (ImGui::Button("Launch Game", { ImGui::GetContentRegionAvail().x, 30 }))
		m_mapper.runGame();
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
