#pragma once

#include "Hook.hpp"

class WindowHooks : public Hook
{
public:
	WindowHooks()
		: Hook("WindowHooks") {};

private:
	bool init() override;
};
