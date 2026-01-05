#pragma once

#include "ILogger.h"

namespace highp::lib::logger {
class TextLogger : public ILogger {
public:
	TextLogger() = default;
	~TextLogger() override = default;

	void Info(std::string_view msg) override;
	void Debug(std::string_view msg) override;
	void Warn(std::string_view msg) override;
	void Error(std::string_view msg) override;
	void Exception(std::string_view msg, std::exception const& ex) override;
};
}
