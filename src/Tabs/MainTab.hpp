#pragma once

#include "Tab.hpp"
#include "Mapper.hpp"

class MainTab : public Tab
{
public:
	MainTab()
		: Tab("Main") {};

	void init() override;
	void render() override;
	bool launchGame();

private:
	bool mapGame();

private:
	Mapper m_mapper;
};
