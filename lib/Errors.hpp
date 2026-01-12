#pragma once

#include <memory>
#include <string_view>
#include "IocpError.h"
#include "Logger.hpp"
#include "Result.hpp"
#include "SocketError.h"

namespace highp::err {
template <auto E>
static std::string_view FromErrorToString() {
	using T = decltype(E);

	if constexpr (std::is_same_v<T, ESocketError>) {
		return FromSocketErrorToString<E>();
	}

	if constexpr (std::is_same_v<T, EIocpError>) {
		return FromIocpErrorToString<E>();
	}
}

template <auto E>
static fn::Result<void, decltype(E)> LogErrorWithResult(std::shared_ptr<log::Logger> logger) {
	using T = decltype(E);
	constexpr const char* ERROR_LOG_FMT = "{}: {}";

	if constexpr (std::is_same_v<T, ESocketError>) {
		logger->Error(ERROR_LOG_FMT, FromSocketErrorToString<E>(), WSAGetLastError());
		return fn::Result<void, T>::Err(E);
	}

	if constexpr (std::is_same_v<T, EIocpError>) {
		logger->Error(ERROR_LOG_FMT, FromIocpErrorToString<E>(), WSAGetLastError());
		return fn::Result<void, T>::Err(E);
	}
}

template <auto E>
static void LogError(std::shared_ptr<log::Logger> logger) {
	using T = decltype(E);
	constexpr const char* ERROR_LOG_FMT = "{}: {}";

	if constexpr (std::is_same_v<T, ESocketError>) {
		logger->Error(ERROR_LOG_FMT, FromSocketErrorToString<E>(), WSAGetLastError());
		return;
	}

	if constexpr (std::is_same_v<T, EIocpError>) {
		logger->Error(ERROR_LOG_FMT, FromIocpErrorToString<E>(), WSAGetLastError());
		return;
	}
}

static decltype(auto) LogErrorByWSAStartupResult(std::shared_ptr<log::Logger> logger) {
	const int err = WSAGetLastError();
	switch (err) {
		case WSANOTINITIALISED: LogError<ESocketError::WsaNotInitialized>(logger); break;
		case WSAENETDOWN: LogError<ESocketError::WsaNetworkSubSystemFailed>(logger); break;
		case WSAEAFNOSUPPORT: LogError<ESocketError::WsaNotSupportedAddressFamily>(logger); break;
		case WSAEINVAL: LogError<ESocketError::WsaInvalidArgs>(logger); break;
		case WSAEMFILE: LogError<ESocketError::WsaLackFileDescriptor>(logger); break;
		case WSAENOBUFS: LogError<ESocketError::WsaNoBufferSpace>(logger); break;
		case WSAEPROTONOSUPPORT: LogError<ESocketError::WsaNotSupportedProtocol>(logger); break;
	}
	return err::LogErrorWithResult<err::ESocketError::CreateSocketFailed>(logger);
}
}
