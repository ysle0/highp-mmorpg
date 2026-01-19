#pragma once

#include <string_view>

namespace highp::err {

/// <summary>
/// IOCP 관련 에러 코드 열거형.
/// IoCompletionPort 및 관련 I/O 작업에서 발생하는 에러를 분류한다.
/// </summary>
enum class EIocpError {
	/// <summary>알 수 없는 에러</summary>
	UnknownError = 0,

	/// <summary>CreateIoCompletionPort() 호출 실패</summary>
	CreateIocpFailed,

	/// <summary>소켓을 IOCP에 연결하는 데 실패</summary>
	ConnectIocpFailed,

	/// <summary>IOCP 내부 에러</summary>
	IocpInternalError,

	/// <summary>I/O 작업 실패</summary>
	IoFailed,

	/// <summary>WSARecv() 호출 실패</summary>
	RecvFailed,

	/// <summary>WSASend() 호출 실패</summary>
	SendFailed,

	/// <summary>Worker 스레드 생성 실패</summary>
	CreateWorkerThreadFailed,

	/// <summary>Accepter 스레드 생성 실패</summary>
	CreateAccepterThreadFailed,

	/// <summary>Worker 스레드 실행 중 에러</summary>
	WorkerThreadFailed,

	/// <summary>Accept 스레드 실행 중 에러</summary>
	AcceptThreadFailed,
};

/// <summary>
/// EIocpError 에러 코드를 문자열로 변환한다.
/// </summary>
/// <param name="e">변환할 EIocpError 값</param>
/// <returns>에러 설명 문자열</returns>
constexpr std::string_view ToString(EIocpError e) {
	switch (e) {
		case EIocpError::CreateIocpFailed: return "CreateIoCompletionPort failed.";
		case EIocpError::RecvFailed: return "Receive failed.";
		case EIocpError::SendFailed: return "Send failed.";
		case EIocpError::CreateWorkerThreadFailed: return "Create worker thread failed.";
		case EIocpError::CreateAccepterThreadFailed: return "Create accepter thread failed.";
		case EIocpError::WorkerThreadFailed: return "Worker thread failed.";
		case EIocpError::AcceptThreadFailed: return "Accept thread failed.";
		case EIocpError::UnknownError:
		default: return "Unknown error.";
	}
}

}
