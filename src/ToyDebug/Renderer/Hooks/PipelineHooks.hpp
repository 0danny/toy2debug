#pragma once

#include "Hook.hpp"

class PipelineHooks : public Hook
{
public:
	PipelineHooks() : Hook("PipelineHooks") {};

	bool init() override;
};
