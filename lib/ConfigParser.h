#pragma once
#include "IConfig.h"

namespace highp::lib::config
{
class ConfigParser final
{
	// static class
private:
	ConfigParser() = delete;
	~ConfigParser() = delete;

public:
	static IConfig Parse(std::string_view configFilePath);

};
}
