#include <Client.h>
#include <Errors.hpp>
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
	_core = std::make_unique<network::ServerCore>(_logger, this);

	if (auto res = _core->Start(asyncSocket, _config); res.HasErr()) {
		return res;
	}

	_logger->Info("EchoServer started on port {}.", _config.server.port);
	return Res::Ok();
}

void EchoServer::Stop() {
	if (_core) {
		_core->Stop();
		_core.reset();
	}
}

void EchoServer::OnAccept(std::shared_ptr<network::Client> client) {
	_logger->Info("[EchoServer] Client accepted: socket #{}", client->socket);
}

void EchoServer::OnRecv(std::shared_ptr<network::Client> client, std::span<const char> data) {
	std::string_view recvData{ data.data(), data.size() };
	_logger->Info("[EchoServer] Recv: socket #{}, data: {}, bytes: {}",
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

void EchoServer::OnSend(std::shared_ptr<network::Client> client, size_t bytesTransferred) {
	_logger->Info("[EchoServer] Send: socket #{}, bytes: {}", client->socket, bytesTransferred);
}

void EchoServer::OnDisconnect(std::shared_ptr<network::Client> client) {
	_logger->Info("[EchoServer] Client disconnected: socket #{}", client->socket);
}

}
