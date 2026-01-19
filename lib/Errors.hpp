#pragma once

#include <memory>
#include <string_view>
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
constexpr std::string_view ErrToString() {
	return ToString(E);
}

/// <summary>
/// 에러를 로깅하고 Result::Err를 반환하는 헬퍼 함수.
/// WSAGetLastError() 값을 함께 출력한다.
/// </summary>
/// <typeparam name="E">에러 코드 값 (컴파일 타임 상수)</typeparam>
/// <param name="logger">로거 인스턴스</param>
/// <returns>에러 코드가 설정된 Result</returns>
template <auto E>
static fn::Result<void, decltype(E)> LogErrorWSAWithResult(std::shared_ptr<log::Logger> logger) {
	logger->Error("{}: {}", ToString(E), WSAGetLastError());
	return fn::Result<void, decltype(E)>::Err(E);
}

template <auto E>
static fn::Result<void, decltype(E)> LogErrorWindowsWithResult(std::shared_ptr<log::Logger> logger) {
	logger->Error("{}: {}", ToString(E), GetLastError());
	return fn::Result<void, decltype(E)>::Err(E);
}

template <auto E>
static fn::Result<void, decltype(E)> LogErrorWithResult(std::shared_ptr<log::Logger> logger) {
	logger->Error("{}", ToString(E));
	return fn::Result<void, decltype(E)>::Err(E);
}

/// <summary>
/// 에러를 로깅하는 헬퍼 함수.
/// WSAGetLastError() 값을 함께 출력한다.
/// </summary>
/// <typeparam name="E">에러 코드 값 (컴파일 타임 상수)</typeparam>
/// <param name="logger">로거 인스턴스</param>
template <auto E>
static void LogErrorWSA(std::shared_ptr<log::Logger> logger) {
	logger->Error("{}: {}", ToString(E), WSAGetLastError());
}

template <auto E>
static void LogErrorWindows(std::shared_ptr<log::Logger> logger) {
	logger->Error("{}: {}", ToString(E), GetLastError());
}

template <auto E>
static void LogError(std::shared_ptr<log::Logger> logger) {
	logger->Error("{}", ToString(E));
}

/// <summary>
/// WSAStartup 실패 시 에러 코드에 따라 적절한 로그를 출력하고 Result를 반환한다.
/// </summary>
/// <param name="logger">로거 인스턴스</param>
/// <returns>CreateSocketFailed 에러가 설정된 Result</returns>
static fn::Result<void, ESocketError> LogErrorByWSAStartupResult(std::shared_ptr<log::Logger> logger) {
	const int err = WSAGetLastError();
	switch (err) {
		case WSANOTINITIALISED: return LogErrorWSAWithResult<ESocketError::WsaNotInitialized>(logger);
		case WSAENETDOWN: return LogErrorWSAWithResult<ESocketError::WsaNetworkSubSystemFailed>(logger);
		case WSAEAFNOSUPPORT: return LogErrorWSAWithResult<ESocketError::WsaNotSupportedAddressFamily>(logger);
		case WSAEINVAL: return LogErrorWSAWithResult<ESocketError::WsaInvalidArgs>(logger);
		case WSAEMFILE: return LogErrorWSAWithResult<ESocketError::WsaLackFileDescriptor>(logger);
		case WSAENOBUFS: return LogErrorWSAWithResult<ESocketError::WsaNoBufferSpace>(logger);
		case WSAEPROTONOSUPPORT: return LogErrorWSAWithResult<ESocketError::WsaNotSupportedProtocol>(logger);
		default: return LogErrorWSAWithResult<ESocketError::WsaStartupFailed>(logger);
	}
}

}
