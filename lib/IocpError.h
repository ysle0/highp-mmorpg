#pragma once

#include "Result.hpp"

namespace highp::lib::error {
enum class EIocpError {
	UnknownError = 0,

	WsaStartupFailed,
	CreateSocketFailed,
	SocketError,
	BindFailed,
	ListenFailed,
	AcceptFailed,
	PostAcceptFailed,

	CreateIocpFailed,
	IocpInternalError,
	IoFailed,

	RecvFailed,
	SendFailed,

	CreateWorkerThreadFailed,
	CreateAccepterThreadFailed,
};

using IocpResult = highp::lib::functional::Result<EIocpError>;

class EIocpErrorHelper {
public:
	static const char* ToString(EIocpError error) {
		switch (error) {
			case EIocpError::WsaStartupFailed: return "WSAStartup failed.";
			case EIocpError::CreateIocpFailed: return "CreateIoCompletionPort failed.";
			case EIocpError::CreateSocketFailed: return "Create socket failed.";
			case EIocpError::SocketError: return "Socket error.";
			case EIocpError::BindFailed: return "Bind failed.";
			case EIocpError::ListenFailed: return "Listen failed.";
			case EIocpError::AcceptFailed: return "Accept failed.";
			case EIocpError::CreateWorkerThreadFailed: return "Create worker thread failed.";
			case EIocpError::CreateAccepterThreadFailed: return "Create accepter thread failed.";
			case EIocpError::PostAcceptFailed: return "Post accept failed.";
			case EIocpError::RecvFailed: return "Receive failed.";
			case EIocpError::SendFailed: return "Send failed.";
			case EIocpError::UnknownError:
			default: return "Unknown error.";
		}
	}
};
}
