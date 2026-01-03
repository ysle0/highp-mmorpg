#pragma once

#include "ILogger.h"
#include <exception>

namespace highp::lib::logger
{
class ConsoleLogger : public logger::ILogger
{
public:
	void Info(std::string_view msg) override;
	void Dbg(std::string_view msg) override;
	void Error(std::string_view msg) override;
	void Exception(std::string_view msg, std::exception const& e) override;
};
}