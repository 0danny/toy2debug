#pragma once

#include "Hook.hpp"

#include <vector>

class DirectX6 final : public Hook
{
public:
	DirectX6()
		: Hook("DirectX6Renderer") {};

	bool init() override;

private:
	void patchRenderTarget();

private:
	std::vector<Hook::SharedPtr> m_hooks;
};
