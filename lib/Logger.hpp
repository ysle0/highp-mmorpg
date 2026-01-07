#pragma once

#include <exception>
#include <format>
#include "ILogger.h"
#include <memory>
#include <string_view>
#include <utility>

// TODO: #1 logger options 추가.
// TODO: #2 logger port (console, file, OTEL ...) 추가
// TODO: #3 logger format (text, json, ...) 추가
namespace highp::log {
class Logger {
	std::unique_ptr<ILogger> _impl;

public:
	explicit Logger(std::unique_ptr<ILogger> impl) : _impl(std::move(impl)) {}

	template <typename Impl>
	static std::shared_ptr<Logger> Default() {
		return std::make_shared<Logger>(std::make_unique<Impl>());
	}

	template <typename Impl, typename... Args>
	static std::shared_ptr<Logger> DefaultWithArgs(Args&&... args) {
		auto impl = std::make_unique<Impl>(std::forward<Args...>(args...));
		return std::make_shared<Logger>(std::move(impl));
	}

	// format string 없이 위임.
	void Info(std::string_view msg) {
		_impl->Info(msg);
	}
	void Debug(std::string_view msg) {
		_impl->Debug(msg);
	}
	void Warn(std::string_view msg) {
		_impl->Warn(msg);
	}
	void Error(std::string_view msg) {
		_impl->Error(msg);
	}
	void Exception(std::string_view msg, std::exception const& ex) {
		_impl->Exception(msg, ex);
	}

	// 템플릿 오버로드
	template<class... Args>
	void Info(std::format_string<Args...> fmt, Args&&... args) {
		_impl->Info(std::vformat(fmt.get(), std::make_format_args(args...)));
	}

	template<class... Args>
	void Debug(std::format_string<Args...> fmt, Args&&... args) {
		_impl->Debug(std::vformat(fmt.get(), std::make_format_args(args...)));
	}

	template<class... Args>
	void Warn(std::format_string<Args...> fmt, Args&&... args) {
		_impl->Warn(std::vformat(fmt.get(), std::make_format_args(args...)));
	}

	template<class... Args>
	void Error(std::format_string<Args...> fmt, Args&&... args) {
		_impl->Error(std::vformat(fmt.get(), std::make_format_args(args...)));
	}
};
}
