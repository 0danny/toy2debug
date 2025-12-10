#pragma once

#include "Hook.hpp"

class DrawHooks : public Hook
{
public:
	DrawHooks()
		: Hook("DrawHooks") {};

	bool init() override;
};
