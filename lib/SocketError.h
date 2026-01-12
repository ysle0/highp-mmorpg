#pragma once

#include <string_view>

namespace highp::err {

/// <summary>
/// 
/// </summary>
enum class ESocketError {
	UnknownError = 0,

	// WSAStartup
	WsaStartupFailed,
	WsaNotInitialized,
	WsaNetworkSubSystemFailed,
	WsaNotSupportedAddressFamily,
	WsaInvalidArgs,
	/// <summary>
	/// too many open sockets.
	/// </summary>
	WsaLackFileDescriptor,
	WsaNoBufferSpace,
	WsaNotSupportedProtocol,

	InvalidSocketErr,
	CreateSocketFailed,

	BindFailed,
	ListenFailed,
	AcceptFailed,
	PostAcceptFailed,
};

template <ESocketError E>
static std::string_view FromSocketErrorToString() {
	switch (E) {
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
