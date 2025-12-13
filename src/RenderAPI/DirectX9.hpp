#pragma once

#include "Hook.hpp"

class DirectX9 final : public Hook
{
public:
	DirectX9()
		: Hook("DirectX9Renderer") {};

	bool init() override;
};
