#pragma once

#include "Hook.hpp"

class DeviceHooks : public Hook
{
public:
	DeviceHooks() : Hook("DeviceHooks") {};

	bool init() override;
};
