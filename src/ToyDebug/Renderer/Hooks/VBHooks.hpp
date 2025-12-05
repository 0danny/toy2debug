#pragma once

#include "Hook.hpp"

class VBHooks : public Hook
{
public:
	VBHooks() : Hook("VertexBufferHooks") {};

	bool init() override;
};
