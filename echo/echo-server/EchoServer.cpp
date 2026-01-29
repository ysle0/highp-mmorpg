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

EchoServer::EchoServer(
	std::shared_ptr<Logger> logger,
	std::shared_ptr<network::SocketOptionBuilder> socketOptionBuilder)
	: _logger(logger)
	, _socketOptionBuilder(socketOptionBuilder)
	, _config(network::NetworkCfg::WithDefaults()) {}

EchoServer::EchoServer(
	std::shared_ptr<Logger> logger,
	network::NetworkCfg config,
	std::shared_ptr<network::SocketOptionBuilder> socketOptionBuilder)
	: _logger(logger)
	, _socketOptionBuilder(socketOptionBuilder)
	, _config(config) {}

EchoServer::Res EchoServer::Start(std::shared_ptr<highp::network::ISocket> listenSocket) {
	_listenSocket = listenSocket;

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
		_socketOptionBuilder,
		_config.server.backlog,
		std::bind_front(&EchoServer::OnAccept, this));

	auto acceptorRes = _acceptor->Initialize(
		listenSocket->GetSocketHandle(),
		_iocp->GetHandle());
	if (acceptorRes.HasErr()) {
		return Res::Err(ENetworkError::IocpCreateFailed);
	}

	for (int i = 0; i < _config.server.maxClients; ++i) {
		_clientPool.emplace_back(std::make_shared<network::Client>());
	}

	auto postRes = _acceptor->PostAccepts(_config.server.backlog);
	if (postRes.HasErr()) {
		return Res::Err(ENetworkError::ThreadAcceptFailed);
	}

	_logger->Info("EchoServer started on port {}.", _config.server.port);
	return Res::Ok();
}

void EchoServer::Stop() {
	// TODO: listenSocket 닫기. Acceptor::Shutdown() 에서 CancelIoEx() 시에 listenSocket
	// 으로 가능한지 확인하기.
	closesocket(_listenSocket->GetSocketHandle());

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
			auto* overlapped = reinterpret_cast<network::AcceptOverlapped*>(event.overlapped);
			if (_acceptor) {
				_logger->Info("[Accept] Socket #{} is accepted", overlapped->clientSocket);
				_acceptor->OnAcceptComplete(overlapped, event.bytesTransferred);
			}
		}
		break;

		case network::EIoType::Recv:
		{
			auto* client = static_cast<network::Client*>(event.completionKey);
			auto clientPtr = client->shared_from_this();
			if (!event.success || event.bytesTransferred == 0) {
				_logger->Info("[Graceful Disconnect] Socket #{} disconnected.", client->socket);
				CloseClient(clientPtr, true);
				break;
			}

			auto* overlapped = reinterpret_cast<network::RecvOverlapped*>(event.overlapped);
			overlapped->buf[event.bytesTransferred] = '\0';

			std::string_view recvData{ overlapped->buf, event.bytesTransferred };
			_logger->Info("[Recv] socket #{}, data: {}, bytes: {}",
				client->socket, recvData, event.bytesTransferred);

			// Echo: 받은 데이터 그대로 전송
			GUARD_EFFECT_BREAK(clientPtr->PostSend(recvData), [&]() {
				CloseClient(clientPtr, true);
			});

			GUARD_EFFECT_BREAK(clientPtr->PostRecv(), [&]() {
				CloseClient(clientPtr, true);
			});
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

	GUARD_EFFECT_VOID(_iocp->AssociateSocket(client->socket, client.get()), [&]() {
		_logger->Error("Failed to associate socket with IOCP.");
		client->Close(true);
	});

	GUARD_EFFECT_VOID(client->PostRecv(), [&]() {
		_logger->Error("Failed to post initial Recv.");
		client->Close(true);
	});

	_connectedClientCount++;

	char clientIp[network::Const::Buffer::clientIpBufferSize]{ 0 };
	inet_ntop(AF_INET, &ctx.remoteAddr.sin_addr, clientIp, sizeof(clientIp));
	_logger->Info("Client connected. socket: {}, ip: {}", client->socket, clientIp);
}

void EchoServer::CloseClient(std::shared_ptr<network::Client> client, bool forceClose) {
	client->Close(forceClose);
	_connectedClientCount--;
}

std::shared_ptr<network::Client> EchoServer::FindAvailableClient() {
	auto found = std::find_if(_clientPool.begin(), _clientPool.end(),
		[](const std::shared_ptr<network::Client>& c) {
		return c->socket == INVALID_SOCKET;
	});
	if (found != _clientPool.end()) {
		return *found;
	}
	return nullptr;
}

}
