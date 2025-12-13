#pragma once

#include "Tab.hpp"

class DebugTab : public Tab
{
public:
	DebugTab()
		: Tab("Debug") {};

	void init() override;
	void render() override;
};