#include "Renderer.hpp"

#include "Hooks/DeviceHooks.hpp"
#include "Hooks/LightsHooks.hpp"
#include "Hooks/VBHooks.hpp"
#include "Hooks/DrawHooks.hpp"
#include "Hooks/StubHooks.hpp"
#include "Hooks/PipelineHooks.hpp"
#include "Hooks/GlueHooks.hpp"
#include "Hooks/TexMatHooks.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_impl_dx9.h>

#include <MinHook.h>
#include <print>

void Renderer::init()
{
	if (MH_Initialize())
	{
		std::println("[Renderer]: Could not init MinHook.");
		return;
	}

	// clang-format off
	m_hooks = {
		std::make_shared<DeviceHooks>(),
		std::make_shared<LightsHooks>(),
		std::make_shared<VBHooks>(),
		std::make_shared<DrawHooks>(),
		std::make_shared<PipelineHooks>(),
		std::make_shared<GlueHooks>(),
		std::make_shared<TexMatHooks>(),
		std::make_shared<StubHooks>()
	};
	// clang-format on

	for (auto& hook : m_hooks)
	{
		bool result = hook->init();

		if (result)
			std::println("[Renderer]: Initialised {}", hook->getName());
		else
			std::println("[Renderer]: Failed to init some/all hooks in {}", hook->getName());
	}

	if (MH_EnableHook(MH_ALL_HOOKS))
		std::println("[Renderer]: Could not enable all hooks.");
}

void Renderer::render()
{
	// Render imgui in here!
}
