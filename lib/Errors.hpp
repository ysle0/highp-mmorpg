#pragma once

#include <string_view>
#include <memory>
#include "Logger.hpp"

namespace highp::err {
template <auto E>
static std::string_view FromErrorToString() {
	using T = decltype(E);

	if constexpr (std::is_same_v<T, ESocketError>) {
		return ::FromIocpErrorToString<T>();
	}

	if constexpr (std::is_same_v<T, EIocpError>) {
		return ::FromIocpErrorToString<T>();
	}
}

template <auto E>
static fn::Result<void, decltype(E)> LogErrorWithResult(std::shared_ptr<log::Logger> logger) {
	using T = decltype(E);
	constexpr const char* ERROR_LOG_FMT = "{}: {}";

	if constexpr (std::is_same_v<T, ESocketError>) {
		logger->Error(ERROR_LOG_FMT, FromSocketErrorToString<T>(), WSAGetLastError());
		return fn::Result::Err(T);
	}

	if constexpr (std::is_same_v<T, EIocpError>) {
		logger->Error(ERROR_LOG_FMT, FromIocpErrorToString<T>(), WSAGetLastError());
		return fn::Result::Err(T);
	}
}

template <auto E>
static void LogError(std::shared_ptr<log::Logger> logger) {
	using T = decltype(E);
	constexpr const char* ERROR_LOG_FMT = "{}: {}";

	if constexpr (std::is_same_v<T, ESocketError>) {
		logger->Error(ERROR_LOG_FMT, FromSocketErrorToString<T>(), WSAGetLastError());
		return;
	}

	if constexpr (std::is_same_v<T, EIocpError>) {
		logger->Error(ERROR_LOG_FMT, FromIocpErrorToString<T>(), WSAGetLastError());
		return;
	}
}
}
