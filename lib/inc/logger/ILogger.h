#pragma once

#include <macro/macro.h>
#include <string_view>
#include <exception>

namespace highp::lib::logger
{
class ILogger {
	CDTOR_INTERFACE(ILogger)

public:
	virtual void Info(std::string_view msg) PURE;
	virtual void Dbg(std::string_view msg) PURE;
	virtual void Error(std::string_view msg) PURE;
	virtual void Exception(std::string_view msg, std::exception const& e) PURE;
};
}