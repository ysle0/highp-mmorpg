#pragma once
#include <string_view>
#include <string>

#include "macro.h"

namespace highp::lib::config
{
class IConfig
{
public:
	virtual ~IConfig() = default;

	virtual std::string Str(std::string_view key, std::string_view defaultValue) const PURE;

	virtual bool Bool(std::string_view key, bool defaultValue) const PURE;

	virtual int Int(std::string_view key, int defaultValue) const PURE;

	virtual long Long(std::string_view key, long defaultValue) const PURE;
};
}
