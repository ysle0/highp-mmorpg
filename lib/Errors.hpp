#pragma once

#include <memory>
#include <string_view>
#include "IocpError.h"
#include "Logger.hpp"
#include "Result.hpp"
#include "SocketError.h"

namespace highp::err {

/// <summary>
/// 에러 코드를 문자열로 변환하는 통합 함수.
/// ESocketError와 EIocpError를 자동으로 구분하여 처리한다.
/// </summary>
/// <typeparam name="E">변환할 에러 코드 값 (컴파일 타임 상수)</typeparam>
/// <returns>에러 설명 문자열</returns>
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

/// <summary>
/// 에러를 로깅하고 Result::Err를 반환하는 헬퍼 함수.
/// WSAGetLastError() 값을 함께 출력한다.
/// </summary>
/// <typeparam name="E">에러 코드 값 (컴파일 타임 상수)</typeparam>
/// <param name="logger">로거 인스턴스</param>
/// <returns>에러 코드가 설정된 Result</returns>
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

/// <summary>
/// 에러를 로깅하는 헬퍼 함수.
/// WSAGetLastError() 값을 함께 출력한다.
/// </summary>
/// <typeparam name="E">에러 코드 값 (컴파일 타임 상수)</typeparam>
/// <param name="logger">로거 인스턴스</param>
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

/// <summary>
/// WSAStartup 실패 시 에러 코드에 따라 적절한 로그를 출력하고 Result를 반환한다.
/// </summary>
/// <param name="logger">로거 인스턴스</param>
/// <returns>CreateSocketFailed 에러가 설정된 Result</returns>
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
