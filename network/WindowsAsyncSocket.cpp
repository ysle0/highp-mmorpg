#include "pch.h"
#include "WindowsAsyncSocket.h"

namespace highp::network {

WindowsAsyncSocket::~WindowsAsyncSocket() {
	WSACleanup();
}

WindowsAsyncSocket::Res WindowsAsyncSocket::Initialize() {
	WSADATA wsaData;
	const int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (res != 0) {
		return err::LogErrorWithResult<err::ESocketError::WsaStartupFailed>(_logger);
	}
	return Res::Ok();
}

WindowsAsyncSocket::Res WindowsAsyncSocket::CreateSocket(NetworkTransport transport) {
	auto [socketType, protocol] = transport.GetInfos();
	_socketHandle = WSASocket(
		AF_INET, /* Address Family (AF_INET, AF_INET6) */
		socketType, /* Socket Type (SOCK_STREAM, SOCK_DGRAM) */
		protocol, /* Protocol (IPPROTO_TCP, IPPROTOUDP) */
		NULL, /* Protocol Information Structure (NULL in most case) */
		0, /* Socket Group */
		WSA_FLAG_OVERLAPPED /* Socket Attribute Flags */
	);
	if (_socketHandle == INVALID_SOCKET) {
		return err::LogErrorByWSAStartupResult(_logger);
	}
	return Res::Ok();
}

WindowsAsyncSocket::Res WindowsAsyncSocket::Bind(unsigned short port) {
	if (!IsValidPort(port)) {
		// TODO: Error Log
		return Res::Ok();
	}

	_sockaddr = {
		.sin_family = AF_INET,
		.sin_port = htons(port)
	};
	_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(_socketHandle, (SOCKADDR*)&addr, sizeof(SOCKADDR_IN)) != 0) {
		return err::LogErrorWithResult<err::ESocketError::BindFailed>(_logger);
	}

	return Res::Ok();
}

WindowsAsyncSocket::Res WindowsAsyncSocket::Listen(int backlog) {
	if (_socketHandle == INVALID_SOCKET) {
		return err::LogErrorWithResult<err::ESocketError::InvalidSocketErr>(_logger);
	}

	if (listen(_socketHandle, backlog) != 0) {
		return err::LogErrorWithResult<err::ESocketError::ListenFailed>(_logger);
	}

	return Res::Ok();
}

WindowsAsyncSocket::Res WindowsAsyncSocket::Accept(SocketHandle clientSocket) {
	char addrBuffer[32];
	const size_t clientSocketAddrSize = sizeof(_sockaddr) + 16;
	OVERLAPPED overlapped{};

	if (BOOL result = AcceptEx(
		_socketHandle, // SocketHandle
		clientSocket, // must not be connected or bound socket.
		&addrBuffer, // local address, remote addres (never be NULL).
		0, // initial data receving buffer on accept.
		clientSocketAddrSize,
		clientSocketAddrSize,
		NULL,
		reinterpret_cast<LPOVERLAPPED>(&overlapped)
	); result != 0) {
		return err::LogErrorWithResult<err::ESocketError::AcceptFailed>(_logger);
	}
	return Res::Ok();
}

WindowsAsyncSocket::Res WindowsAsyncSocket::Cleanup() {
	int status = closesocket(_socketHandle);
	status = WSACleanup();

	return Res::Ok();
}

}
