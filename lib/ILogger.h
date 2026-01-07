#pragma once

#include <string_view>
#include <exception>

#include "macro.h"

namespace highp::log {
class ILogger {
public:
	virtual ~ILogger() = default;
	virtual void Info(std::string_view) PURE;
	virtual void Debug(std::string_view) PURE;
	virtual void Warn(std::string_view) PURE;
	virtual void Error(std::string_view) PURE;
	virtual void Exception(std::string_view, std::exception const&) PURE;
};
}
