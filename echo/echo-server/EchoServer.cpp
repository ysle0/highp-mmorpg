#include <Client.h>
#include <Errors.hpp>
#include <OverlappedExt.h>
#include "EchoServer.h"

namespace highp::echo_srv {
using namespace highp::err;
using namespace highp::log;

EchoServer::~EchoServer() noexcept {
	Stop();
	WSACleanup();
}

EchoServer::EchoServer(std::shared_ptr<Logger> logger)
	: _logger(logger)
	, _config(network::NetworkCfg::WithDefaults()) {}

EchoServer::EchoServer(std::shared_ptr<Logger> logger, network::NetworkCfg config)
	: _logger(logger)
	, _config(config) {}

EchoServer::Res EchoServer::Start(std::shared_ptr<highp::network::ISocket> asyncSocket) {
	_iocp = std::make_unique<network::IoCompletionPort>(
		_logger,
		std::bind_front(&EchoServer::OnCompletion, this));

	const auto iocpWorkerCount = std::thread::hardware_concurrency() *
		_config.thread.maxWorkerThreadMultiplier;
	if (auto res = _iocp->Initialize(iocpWorkerCount); res.HasErr()) {
		return res;
	}

	_acceptor = std::make_unique<network::Acceptor>(
		_logger,
		_config.server.backlog,
		std::bind_front(&EchoServer::OnAccept, this));

	auto acceptorRes = _acceptor->Initialize(
		asyncSocket->GetSocketHandle(),
		_iocp->GetHandle());
	if (acceptorRes.HasErr()) {
		return Res::Err(EIocpError::CreateIocpFailed);
	}

	for (int i = 0; i < _config.server.maxClients; ++i) {
		_clientPool.emplace_back(std::make_shared<network::Client>());
	}

	auto postRes = _acceptor->PostAccepts(_config.server.backlog);
	if (postRes.HasErr()) {
		return Res::Err(EIocpError::AcceptThreadFailed);
	}

	_logger->Info("EchoServer started on port {}.", _config.server.port);
	return Res::Ok();
}

void EchoServer::Stop() {
	if (_acceptor) {
		_acceptor->Shutdown();
		_acceptor.reset();
	}

	if (_iocp) {
		_iocp->Shutdown();
		_iocp.reset();
	}

	_clientPool.clear();
	_connectedClientCount = 0;
}

void EchoServer::OnCompletion(network::CompletionEvent event) {
	switch (event.ioType) {
		case network::EIoType::Accept:
		{
			auto* overlapped = reinterpret_cast<network::OverlappedExt*>(event.overlapped);
			if (_acceptor) {
				_logger->Info("[Accept] Socket #{} is accepted", overlapped->clientSocket);
				_acceptor->OnAcceptComplete(overlapped, event.bytesTransferred);
			}
		}
		break;

		case network::EIoType::Recv:
		{
			auto* client = static_cast<network::Client*>(event.completionKey);
			if (!event.success || event.bytesTransferred == 0) {
				_logger->Info("[Graceful Disconnect] Socket #{} disconnected.", client->socket);
				CloseSocket(client->shared_from_this(), true);
				break;
			}

			auto* overlapped = reinterpret_cast<network::OverlappedExt*>(event.overlapped);
			overlapped->recvBuffer[event.bytesTransferred] = '\0';

			_logger->Info("[Recv] socket #{}, data: {}, bytes: {}",
				client->socket,
				std::string_view(overlapped->recvBuffer, event.bytesTransferred),
				event.bytesTransferred);

			auto clientPtr = client->shared_from_this();
			std::string_view recvStr{
				overlapped->recvBuffer,
				event.bytesTransferred };
			if (auto res = Send(clientPtr, recvStr, event.bytesTransferred); res.HasErr()) {
				CloseSocket(clientPtr, true);
				break;
			}

			if (auto res = Recv(clientPtr); res.HasErr()) {
				CloseSocket(clientPtr, true);
			}
		}
		break;

		case network::EIoType::Send:
		{
			auto* client = static_cast<network::Client*>(event.completionKey);
			_logger->Info("[Send] socket #{}, bytes: {}", client->socket, event.bytesTransferred);
		}
		break;

		default:
		_logger->Error("Unknown IO type received.");
		break;
	}
}

void EchoServer::OnAccept(network::AcceptContext& ctx) {
	auto client = FindAvailableClient();
	if (!client) {
		_logger->Error("Client pool full!");
		closesocket(ctx.acceptSocket);
		return;
	}

	client->socket = ctx.acceptSocket;

	if (auto res = _iocp->AssociateSocket(client->socket, client.get()); res.HasErr()) {
		_logger->Error("Failed to associate socket with IOCP.");
		client->Close(true);
		return;
	}

	if (auto res = Recv(client); res.HasErr()) {
		_logger->Error("Failed to post initial Recv.");
		client->Close(true);
		return;
	}

	_connectedClientCount++;

	char clientIp[network::Const::Network::clientIpBufferSize]{ 0 };
	inet_ntop(AF_INET, &ctx.remoteAddr.sin_addr, clientIp, sizeof(clientIp));
	_logger->Info("Client connected. socket: {}, ip: {}", client->socket, clientIp);
}

EchoServer::Res EchoServer::Recv(std::shared_ptr<network::Client> clientSocket) {
	ZeroMemory(&clientSocket->recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));

	clientSocket->recvOverlapped.wsaBuffer.buf = clientSocket->recvOverlapped.recvBuffer;
	clientSocket->recvOverlapped.wsaBuffer.len = network::Const::Socket::recvBufferSize;
	clientSocket->recvOverlapped.ioType = network::EIoType::Recv;

	DWORD flag = 0;
	DWORD recvNumBytes = 0;

	int result = WSARecv(
		clientSocket->socket,
		&clientSocket->recvOverlapped.wsaBuffer,
		1,
		&recvNumBytes,
		&flag,
		reinterpret_cast<LPWSAOVERLAPPED>(&clientSocket->recvOverlapped),
		nullptr);

	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
		return err::LogErrorWithResult<EIocpError::RecvFailed>(_logger);
	}
	return Res::Ok();
}

EchoServer::Res EchoServer::Send(
	std::shared_ptr<network::Client> clientSocket,
	std::string_view message,
	ULONG messageLength
) {
	ZeroMemory(&clientSocket->sendOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	CopyMemory(clientSocket->sendOverlapped.sendBuffer, message.data(), messageLength);

	clientSocket->sendOverlapped.wsaBuffer.len = messageLength;
	clientSocket->sendOverlapped.wsaBuffer.buf = clientSocket->sendOverlapped.sendBuffer;
	clientSocket->sendOverlapped.ioType = network::EIoType::Send;

	DWORD sendNumBytes = 0;

	int result = WSASend(
		clientSocket->socket,
		&clientSocket->sendOverlapped.wsaBuffer,
		1,
		&sendNumBytes,
		0,
		reinterpret_cast<LPWSAOVERLAPPED>(&clientSocket->sendOverlapped),
		nullptr);

	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
		return err::LogErrorWithResult<EIocpError::SendFailed>(_logger);
	}

	return Res::Ok();
}

void EchoServer::CloseSocket(std::shared_ptr<network::Client> clientSocket, bool forceClose) {
	clientSocket->Close(forceClose);
	_connectedClientCount--;
}

std::shared_ptr<network::Client> EchoServer::FindAvailableClient() {
	auto found = std::find_if(_clientPool.begin(), _clientPool.end(), [](const std::shared_ptr<network::Client>& c) {
		return c->socket == INVALID_SOCKET;
		});
	if (found != _clientPool.end()) {
		return *found;
	}
	return nullptr;
}

}
