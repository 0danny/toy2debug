#include "DebugTab.hpp"

#include <ImGui/imgui.h>

void DebugTab::init()
{
	// Debug tab init
}

void DebugTab::render()
{
	// Debug tab
	ImGui::Text("Debug Tab!");

	ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("FPS: %.1f", io.Framerate);
}