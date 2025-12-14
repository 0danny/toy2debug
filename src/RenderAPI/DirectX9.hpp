#pragma once

#include "Hook.hpp"
#include <vector>

class DirectX9 final : public Hook
{
public:
	DirectX9()
		: Hook("DirectX9Renderer") {};

	bool init() override;

private:
	std::vector<Hook::SharedPtr> m_hooks;
};
