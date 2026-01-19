#pragma once

#include <string_view>

namespace highp::err {

/// <summary>
/// 소켓 관련 에러 코드 열거형.
/// Winsock 초기화, 소켓 생성, 바인딩, Listen, Accept 등의 에러를 분류한다.
/// </summary>
enum class ESocketError {
	/// <summary>알 수 없는 에러</summary>
	UnknownError = 0,

	// WSAStartup 관련 에러
	/// <summary>WSAStartup() 호출 실패</summary>
	WsaStartupFailed,

	/// <summary>WSAStartup()이 호출되지 않음</summary>
	WsaNotInitialized,

	/// <summary>네트워크 서브시스템 실패</summary>
	WsaNetworkSubSystemFailed,

	/// <summary>지원하지 않는 주소 패밀리 (AF_INET, AF_INET6 등)</summary>
	WsaNotSupportedAddressFamily,

	/// <summary>잘못된 인자</summary>
	WsaInvalidArgs,

	/// <summary>열린 소켓이 너무 많음 (파일 디스크립터 부족)</summary>
	WsaLackFileDescriptor,

	/// <summary>버퍼 공간 부족</summary>
	WsaNoBufferSpace,

	/// <summary>지원하지 않는 프로토콜</summary>
	WsaNotSupportedProtocol,

	// 소켓 생성 관련 에러
	/// <summary>유효하지 않은 소켓</summary>
	InvalidSocketErr,

	/// <summary>소켓 생성 실패</summary>
	CreateSocketFailed,

	// 소켓 동작 관련 에러
	/// <summary>bind() 호출 실패</summary>
	BindFailed,

	/// <summary>listen() 호출 실패</summary>
	ListenFailed,

	/// <summary>accept() 호출 실패</summary>
	AcceptFailed,

	/// <summary>AcceptEx() 호출 실패</summary>
	PostAcceptFailed,
};

/// <summary>
/// ESocketError 에러 코드를 문자열로 변환한다.
/// </summary>
/// <param name="e">변환할 ESocketError 값</param>
/// <returns>에러 설명 문자열</returns>
constexpr std::string_view ToString(ESocketError e) {
	switch (e) {
		case ESocketError::WsaStartupFailed: return "WSAStartup failed.";
		case ESocketError::WsaNotInitialized: return "WSAStartup failed: WSAStartup() is not called.";
		case ESocketError::WsaNetworkSubSystemFailed: return "WSAStartup failed: Network Subsystem failed.";
		case ESocketError::WsaNotSupportedAddressFamily: return "WSAStartup failed: Not supported Address Family (e.g. AF_INET, AF_INET6)";
		case ESocketError::WsaInvalidArgs: return "WSAStartup failed: Invalid Arguments.";
		case ESocketError::WsaNoBufferSpace: return "WSAStartup failed: Not enough buffer space for a socket.";
		case ESocketError::WsaNotSupportedProtocol: return "WSAStartup failed: Not supported protocol.";
		case ESocketError::InvalidSocketErr: return "Invalid socket.";
		case ESocketError::CreateSocketFailed: return "Create socket failed.";
		case ESocketError::BindFailed: return "Bind failed.";
		case ESocketError::ListenFailed: return "Listen failed.";
		case ESocketError::AcceptFailed: return "Accept failed.";
		case ESocketError::PostAcceptFailed: return "Post accept failed.";
		case ESocketError::UnknownError:
		default: return "Unknown error.";
	}
}

}
