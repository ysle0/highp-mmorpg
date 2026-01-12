#pragma once

#include <string_view>

namespace highp::err {
enum class EIocpError {
	UnknownError = 0,

	CreateIocpFailed,
	ConnectIocpFailed,
	IocpInternalError,
	IoFailed,

	RecvFailed,
	SendFailed,

	CreateWorkerThreadFailed,
	CreateAccepterThreadFailed,
	WorkerThreadFailed,
	AcceptThreadFailed,
};

//static std::string_view FromIocpErrorToString(EIocpError err) {
//	switch (err) {
//		case EIocpError::CreateIocpFailed: return "CreateIoCompletionPort failed.";
//		case EIocpError::ConnectIocpFailed: return "CreateIoCompletionPort failed: connecting a clientSocket socket failed.";
//		case EIocpError::RecvFailed: return "Receive failed.";
//		case EIocpError::SendFailed: return "Send failed.";
//		case EIocpError::CreateWorkerThreadFailed: return "Create worker thread failed.";
//		case EIocpError::CreateAccepterThreadFailed: return "Create accepter thread failed.";
//		case EIocpError::WorkerThreadFailed: return "Worker thread failed.";
//		case EIocpError::AcceptThreadFailed: return "Accept thread failed.";
//		case EIocpError::UnknownError:
//		default: return "Unknown error.";
//	}
//}

template <EIocpError E>
static std::string_view FromIocpErrorToString() {
	switch (E) {
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
