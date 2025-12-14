#pragma once

#include "Hook.hpp"

namespace DX6
{
	class DeviceHooks : public Hook
	{
	public:
		DeviceHooks()
			: Hook("DeviceHooks") {};

		bool init() override;
	};
}
