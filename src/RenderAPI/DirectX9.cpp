#include "DirectX9.hpp"

#include "Hooks/DX9/DeviceHooks.hpp"
#include "Hooks/DX9/LightsHooks.hpp"
#include "Hooks/DX9/VBHooks.hpp"
#include "Hooks/DX9/DrawHooks.hpp"
#include "Hooks/DX9/StubHooks.hpp"
#include "Hooks/DX9/PipelineHooks.hpp"
#include "Hooks/DX9/GlueHooks.hpp"
#include "Hooks/DX9/TexMatHooks.hpp"

#include <print>

bool DirectX9::init()
{
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
			std::println("[DirectX9]: Initialised {}", hook->getName());
		else
			std::println("[DirectX9]: Failed to init some/all hooks in {}", hook->getName());
	}

	return true;
}