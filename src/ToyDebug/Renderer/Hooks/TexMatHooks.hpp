#pragma once

#include "Hook.hpp"

class TexMatHooks : public Hook
{
public:
	TexMatHooks()
		: Hook("TextureMaterialHooks") {};

	bool init() override;
};
