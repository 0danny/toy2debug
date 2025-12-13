#include "DirectX3.hpp"

#include "Hooks/DX3/DeviceHooks.hpp"

#include <print>

bool DirectX3::init()
{
	m_hooks = {
		std::make_shared<DeviceHooks>(),
	};

	bool init = true;

	for (auto& hook : m_hooks)
	{
		if (! hook->init())
		{
			std::println("[DirectX3]: Hook {} failed.", hook->getName());
			init = false;
		}
	}

	return init;
}