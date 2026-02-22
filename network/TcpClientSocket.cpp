#include "pch.h"
#include "TcpClientSocket.h"
#include <climits>
#include <string>
#include <Errors.hpp>
#include "NetworkUtil.hpp"
#include "WsaSession.h"

namespace highp::network {

TcpClientSocket::TcpClientSocket(
	std::shared_ptr<log::Logger> logger,
	std::shared_ptr<WsaSession> wsaSession
)
	: _logger(std::move(logger))
	, _wsaSession(std::move(wsaSession)) {
	//
}

TcpClientSocket::~TcpClientSocket() noexcept {
	Close();
}

TcpClientSocket::Res TcpClientSocket::Connect(std::string_view ipAddress, unsigned short port) {
	if (!_wsaSession) {
		return err::LogErrorWithResult<err::ENetworkError::WsaNotInitialized>(_logger);
	}

	Close();

	if (!IsValidPort(port)) {
		return err::LogErrorWithResult<err::ENetworkError::WsaInvalidArgs>(_logger);
	}

	_socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_socketHandle == InvalidSocket) {
		return err::LogErrorWSAWithResult<err::ENetworkError::SocketCreateFailed>(_logger);
	}

	SOCKADDR_IN address{
		.sin_family = AF_INET,
		.sin_port = htons(port)
	};

	std::string ipAddressString{ ipAddress };
	if (InetPtonA(AF_INET, ipAddressString.c_str(), &address.sin_addr) != 1) {
		Close();
		return err::LogErrorWithResult<err::ENetworkError::WsaInvalidArgs>(_logger);
	}

	if (connect(_socketHandle, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) == SocketError) {
		Close();
		return err::LogErrorWSAWithResult<err::ENetworkError::SocketConnectFailed>(_logger);
	}

	return Res::Ok();
}

TcpClientSocket::Res TcpClientSocket::SendAll(std::span<const char> data) {
	if (!IsConnected()) {
		return err::LogErrorWithResult<err::ENetworkError::SocketInvalid>(_logger);
	}

	size_t sentBytes = 0;
	while (sentBytes < data.size()) {
		const size_t remain = data.size() - sentBytes;
		const size_t currentChunkSize = (remain < static_cast<size_t>(INT_MAX))
			? remain
			: static_cast<size_t>(INT_MAX);
		const int sent = send(
			_socketHandle,
			data.data() + sentBytes,
			static_cast<int>(currentChunkSize),
			0);

		if (sent == SocketError || sent <= 0) {
			return err::LogErrorWSAWithResult<err::ENetworkError::WsaSendFailed>(_logger);
		}

		sentBytes += static_cast<size_t>(sent);
	}

	return Res::Ok();
}

TcpClientSocket::ResWithSize TcpClientSocket::RecvSome(std::span<char> buffer) {
	if (!IsConnected()) {
		return ResWithSize::Err(err::ENetworkError::SocketInvalid);
	}

	if (buffer.empty()) {
		return ResWithSize::Ok(0);
	}

	const int recvBytes = recv(_socketHandle, buffer.data(), static_cast<int>(buffer.size()), 0);
	if (recvBytes == SocketError) {
		err::LogErrorWSA<err::ENetworkError::WsaRecvFailed>(_logger);
		return ResWithSize::Err(err::ENetworkError::WsaRecvFailed);
	}

	return ResWithSize::Ok(static_cast<size_t>(recvBytes));
}

TcpClientSocket::Res TcpClientSocket::Close() {
	if (_socketHandle == InvalidSocket) {
		return Res::Ok();
	}

	shutdown(_socketHandle, SD_BOTH);
	closesocket(_socketHandle);
	_socketHandle = InvalidSocket;
	return Res::Ok();
}

bool TcpClientSocket::IsConnected() const noexcept {
	return _socketHandle != InvalidSocket;
}

SocketHandle TcpClientSocket::GetSocketHandle() const noexcept {
	return _socketHandle;
}

}
