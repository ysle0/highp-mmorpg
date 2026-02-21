#include <Client.h>
#include <Errors.hpp>
#include "EchoServer.h"

Server::~Server() noexcept {
	Stop();
}

Server::Server(
	std::shared_ptr<log::Logger> logger,
	network::NetworkCfg config,
	std::shared_ptr<network::SocketOptionBuilder> socketOptionBuilder
)
	: _logger(logger),
	_config(config),
	_socketOptionBuilder(socketOptionBuilder) {
	//
}

Server::Res Server::Start(std::shared_ptr<highp::network::ISocket> asyncSocket) {
	_core = std::make_unique<network::ServerLifeCycle>(_logger, _socketOptionBuilder, this);

	GUARD(_core->Start(asyncSocket, _config));

	_logger->Info("Server started on port {}.", _config.server.port);
	return Res::Ok();
}

void Server::Stop() {
	if (_core) {
		_core->Stop();
		_core.reset();
	}
}

void Server::OnAccept(std::shared_ptr<network::Client> client) {
	_logger->Info("[Server::OnAccept] Client accepted: socket #{}", client->socket);
}

void Server::OnRecv(std::shared_ptr<network::Client> client, std::span<const char> data) {
	std::string_view recvData{ data.data(), data.size() };
	_logger->Info("[Server::OnRecv] Recv: socket #{}, data: {}, bytes: {}",
		client->socket, recvData, data.size());

	// Echo: 받은 데이터 그대로 전송
	if (auto res = client->PostSend(recvData); res.HasErr()) {
		_core->CloseClient(client, true);
		return;
	}

	// 다음 수신 대기
	if (auto res = client->PostRecv(); res.HasErr()) {
		_core->CloseClient(client, true);
	}
}

void Server::OnSend(std::shared_ptr<network::Client> client, size_t bytesTransferred) {
	_logger->Info("[Server::OnSend] Send: socket #{}, bytes: {}", client->socket, bytesTransferred);
}

void Server::OnDisconnect(std::shared_ptr<network::Client> client) {
	_logger->Info("[Server::OnDisconnect] Client disconnected: socket #{}", client->socket);
}
