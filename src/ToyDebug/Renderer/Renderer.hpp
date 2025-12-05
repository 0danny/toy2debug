#pragma once

#include "ToyTypes.hpp"
#include "Hook.hpp"

#include <vector>

class Renderer
{
public:
	void init();
	void render();

private:
	std::vector<Hook::SharedPtr> m_hooks;
};
