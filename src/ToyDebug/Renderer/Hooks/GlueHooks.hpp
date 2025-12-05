#pragma once

#include "Hook.hpp"

class GlueHooks : public Hook
{
public:
	GlueHooks() : Hook("GlueHooks") {};

	bool init() override;
};
