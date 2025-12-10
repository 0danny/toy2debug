#pragma once

#include "Hook.hpp"

class LightsHooks : public Hook
{
public:
	LightsHooks()
		: Hook("LightsHooks") {};

	bool init() override;
};
