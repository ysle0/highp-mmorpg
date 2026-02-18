#pragma once

#include <string_view>

namespace highp::err {

/// <summary>
/// 네트워크 관련 에러 코드 열거형.
/// 소켓, IOCP, I/O 작업에서 발생하는 에러를 분류한다.
/// </summary>
enum class ENetworkError {
	/// <summary>알 수 없는 에러</summary>
	UnknownError = 0,

	// ========== WSA 관련 ==========
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
	/// <summary>WSARecv() 호출 실패</summary>
	WsaRecvFailed,
	/// <summary>WSASend() 호출 실패</summary>
	WsaSendFailed,

	// ========== 소켓 관련 ==========
	/// <summary>유효하지 않은 소켓</summary>
	SocketInvalid,
	/// <summary>소켓 생성 실패</summary>
	SocketCreateFailed,
	/// <summary>bind() 호출 실패</summary>
	SocketBindFailed,
	/// <summary>listen() 호출 실패</summary>
	SocketListenFailed,
	/// <summary>accept() 호출 실패</summary>
	SocketAcceptFailed,
	/// <summary>connect() 호출 실패</summary>
	SocketConnectFailed,
	/// <summary>AcceptEx() 호출 실패</summary>
	SocketPostAcceptFailed,

	// ========== IOCP 관련 ==========
	/// <summary>CreateIoCompletionPort() 호출 실패</summary>
	IocpCreateFailed,
	/// <summary>소켓을 IOCP에 연결하는 데 실패</summary>
	IocpConnectFailed,
	/// <summary>IOCP 내부 에러</summary>
	IocpInternalError,
	/// <summary>I/O 작업 실패</summary>
	IocpIoFailed,

	// ========== 스레드 관련 ==========
	/// <summary>Worker 스레드 생성 실패</summary>
	ThreadWorkerCreateFailed,
	/// <summary>Accepter 스레드 생성 실패</summary>
	ThreadAccepterCreateFailed,
	/// <summary>Worker 스레드 실행 중 에러</summary>
	ThreadWorkerFailed,
	/// <summary>Accept 스레드 실행 중 에러</summary>
	ThreadAcceptFailed,
};

/// <summary>
/// ENetworkError 에러 코드를 문자열로 변환한다.
/// </summary>
/// <param name="e">변환할 ENetworkError 값</param>
/// <returns>에러 설명 문자열</returns>
constexpr std::string_view ToString(ENetworkError e) {
	switch (e) {
		// WSA 관련
		case ENetworkError::WsaStartupFailed: return "WSAStartup failed.";
		case ENetworkError::WsaNotInitialized: return "WSAStartup failed: WSAStartup() is not called.";
		case ENetworkError::WsaNetworkSubSystemFailed: return "WSAStartup failed: Network Subsystem failed.";
		case ENetworkError::WsaNotSupportedAddressFamily: return "WSAStartup failed: Not supported Address Family (e.g. AF_INET, AF_INET6)";
		case ENetworkError::WsaInvalidArgs: return "WSAStartup failed: Invalid Arguments.";
		case ENetworkError::WsaNoBufferSpace: return "WSAStartup failed: Not enough buffer space for a socket.";
		case ENetworkError::WsaNotSupportedProtocol: return "WSAStartup failed: Not supported protocol.";
		case ENetworkError::WsaRecvFailed: return "WSARecv failed.";
		case ENetworkError::WsaSendFailed: return "WSASend failed.";

		// 소켓 관련
		case ENetworkError::SocketInvalid: return "Invalid socket.";
		case ENetworkError::SocketCreateFailed: return "Create socket failed.";
		case ENetworkError::SocketBindFailed: return "Bind failed.";
		case ENetworkError::SocketListenFailed: return "Listen failed.";
		case ENetworkError::SocketAcceptFailed: return "Accept failed.";
		case ENetworkError::SocketConnectFailed: return "Connect failed.";
		case ENetworkError::SocketPostAcceptFailed: return "Post accept failed.";

		// IOCP 관련
		case ENetworkError::IocpCreateFailed: return "CreateIoCompletionPort failed.";
		case ENetworkError::IocpConnectFailed: return "Connect to IOCP failed.";
		case ENetworkError::IocpInternalError: return "IOCP internal error.";
		case ENetworkError::IocpIoFailed: return "I/O operation failed.";

		// 스레드 관련
		case ENetworkError::ThreadWorkerCreateFailed: return "Create worker thread failed.";
		case ENetworkError::ThreadAccepterCreateFailed: return "Create accepter thread failed.";
		case ENetworkError::ThreadWorkerFailed: return "Worker thread failed.";
		case ENetworkError::ThreadAcceptFailed: return "Accept thread failed.";

		case ENetworkError::UnknownError:
		default: return "Unknown error.";
	}
}

}
