#pragma once

#include "Hook.hpp"

#include <vector>

class DirectX3 final : public Hook
{
public:
	DirectX3()
		: Hook("DirectX3Renderer") {};

	bool init() override;

private:
	std::vector<Hook::SharedPtr> m_hooks;
};
