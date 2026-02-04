#include "pch.h"
#include "ServerCore.h"
#include "OverlappedExt.h"
#include "Const.h"
#include <Errors.hpp>

namespace highp::network {

ServerCore::ServerCore(
	std::shared_ptr<log::Logger> logger,
	IServerHandler* handler)
	: _logger(std::move(logger))
	, _handler(handler)
{
}

ServerCore::~ServerCore() noexcept {
	Stop();
}

ServerCore::Res ServerCore::Start(std::shared_ptr<ISocket> listenSocket, const NetworkCfg& config) {
	_config = config;

	// IOCP 초기화
	_iocp = std::make_unique<IocpIoMultiplexer>(
		_logger,
		std::bind_front(&ServerCore::OnCompletion, this));

	const auto workerCount = std::thread::hardware_concurrency() *
		_config.thread.maxWorkerThreadMultiplier;
	if (auto res = _iocp->Initialize(workerCount); res.HasErr()) {
		return res;
	}

	// Acceptor 초기화
	_acceptor = std::make_unique<IocpAcceptor>(
		_logger,
		_config.server.backlog,
		std::bind_front(&ServerCore::OnAcceptInternal, this));

	if (auto res = _acceptor->Initialize(listenSocket->GetSocketHandle(), _iocp->GetHandle()); res.HasErr()) {
		return Res::Err(err::ENetworkError::IocpCreateFailed);
	}

	// Client 풀 사전 할당
	_clientPool.reserve(_config.server.maxClients);
	for (int i = 0; i < _config.server.maxClients; ++i) {
		_clientPool.emplace_back(std::make_shared<Client>());
	}

	// Accept 요청 등록
	if (auto res = _acceptor->PostAccepts(_config.server.backlog); res.HasErr()) {
		return Res::Err(err::ENetworkError::ThreadAcceptFailed);
	}

	_logger->Info("ServerCore started on port {}.", _config.server.port);
	return Res::Ok();
}

void ServerCore::Stop() {
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

void ServerCore::CloseClient(std::shared_ptr<Client> client, bool force) {
	if (client && client->socket != INVALID_SOCKET) {
		client->Close(force);
		_connectedClientCount--;
	}
}

void ServerCore::OnCompletion(CompletionEvent event) {
	switch (event.ioType) {
		case EIoType::Accept:
		{
			auto* overlapped = reinterpret_cast<OverlappedExt*>(event.overlapped);
			if (_acceptor) {
				_acceptor->OnAcceptComplete(overlapped, event.bytesTransferred);
			}
		}
		break;

		case EIoType::Recv:
			HandleRecv(event);
			break;

		case EIoType::Send:
			HandleSend(event);
			break;

		default:
			_logger->Error("Unknown IO type received.");
			break;
	}
}

void ServerCore::OnAcceptInternal(AcceptContext& ctx) {
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

	if (auto res = client->PostRecv(); res.HasErr()) {
		_logger->Error("Failed to post initial Recv.");
		client->Close(true);
		return;
	}

	_connectedClientCount++;

	char clientIp[Const::Network::clientIpBufferSize]{ 0 };
	inet_ntop(AF_INET, &ctx.remoteAddr.sin_addr, clientIp, sizeof(clientIp));
	_logger->Info("Client connected. socket: {}, ip: {}", client->socket, clientIp);

	// 앱 레이어에 통지
	if (_handler) {
		_handler->OnAccept(client);
	}
}

void ServerCore::HandleRecv(CompletionEvent& event) {
	auto* client = static_cast<Client*>(event.completionKey);
	if (!client) return;

	auto clientPtr = client->shared_from_this();

	// 연결 종료 감지
	if (!event.success || event.bytesTransferred == 0) {
		_logger->Info("[Graceful Disconnect] Socket #{} disconnected.", client->socket);
		if (_handler) {
			_handler->OnDisconnect(clientPtr);
		}
		CloseClient(clientPtr, true);
		return;
	}

	auto* overlapped = reinterpret_cast<OverlappedExt*>(event.overlapped);
	std::span<const char> data{ overlapped->recvBuffer, event.bytesTransferred };

	// 앱 레이어에 통지
	if (_handler) {
		_handler->OnRecv(clientPtr, data);
	}
}

void ServerCore::HandleSend(CompletionEvent& event) {
	auto* client = static_cast<Client*>(event.completionKey);
	if (!client) return;

	auto clientPtr = client->shared_from_this();

	// 앱 레이어에 통지
	if (_handler) {
		_handler->OnSend(clientPtr, event.bytesTransferred);
	}
}

std::shared_ptr<Client> ServerCore::FindAvailableClient() {
	auto found = std::find_if(_clientPool.begin(), _clientPool.end(),
		[](const std::shared_ptr<Client>& c) {
			return c->socket == INVALID_SOCKET;
		});
	if (found != _clientPool.end()) {
		return *found;
	}
	return nullptr;
}

} // namespace highp::network
