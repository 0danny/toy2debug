#pragma once

#include "Hook.hpp"

class MiscHooks : public Hook
{
public:
	MiscHooks()
		: Hook("MiscHooks") {};

	bool init() override;
};
