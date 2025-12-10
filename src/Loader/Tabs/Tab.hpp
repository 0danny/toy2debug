#pragma once

#include <string>
#include <memory>

class Tab
{
public:
	using SharedPtr = std::shared_ptr<Tab>;

	Tab(std::string tabName) : m_tabName(tabName) {};

	virtual void render() = 0;

	const std::string& getName() { return m_tabName; }

private:
	std::string m_tabName;
};
