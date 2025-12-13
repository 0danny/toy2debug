#include "MainTab.hpp"
#include "Settings.hpp"

// Hooks
#include "RenderAPI/DirectX3.hpp"
#include "RenderAPI/DirectX9.hpp"
#include "Hooks/CommonHooks.hpp"
#include "Hooks/WindowHooks.hpp"

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

void MainTab::init()
{
	// Main tab init
	std::ifstream file("game_path.txt");

	if (file)
	{
		std::getline(file, m_gamePath);
		// should be populated from settings file
		std::strncpy(g_settings.gamePath, m_gamePath.c_str(), sizeof(g_settings.gamePath));
	}
}

void MainTab::render()
{
	// Game Patches
	ImGui::SeparatorText("Game Patches");

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
	ImGui::InputText("##game_path", m_gamePath.data(), m_gamePath.length(), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();

	ImGui::Button("Change Path", { ImGui::GetContentRegionAvail().x, 30 });

	if (ImGui::Button("Map Game", { ImGui::GetContentRegionAvail().x, 30 }))
	{
		std::println("Mapping!");
		m_mapper.map(m_gamePath);
	}

	if (ImGui::Button("Launch Game", { ImGui::GetContentRegionAvail().x, 30 }))
		launchGame();
}

void MainTab::launchGame()
{
	std::println("Launch Game!");

	std::vector<Hook::SharedPtr> hooks = {
		std::make_shared<CommonHooks>(),
		std::make_shared<WindowHooks>(),
	};

	switch (g_settings.renderApi)
	{
		case Settings::RenderAPI::DirectX3:
			hooks.push_back(std::make_shared<DirectX3>());
		case Settings::RenderAPI::DirectX9:
			hooks.push_back(std::make_shared<DirectX9>());
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
			std::println("Initalised [{}] hook", hook->getName());
		else
			std::println("Failed to initialise [{}] hook", hook->getName());
	}

	if (MH_EnableHook(MH_ALL_HOOKS))
		std::println("[Renderer]: Could not enable all hooks.");

	// start the game
	m_mapper.runMap();
}
