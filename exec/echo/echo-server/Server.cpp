#include "Server.h"
#include <Client.h>
#include <Errors.hpp>

Server::~Server() noexcept { Stop(); }

Server::Server(
    std::shared_ptr<log::Logger> logger,
    network::NetworkCfg config,
    std::shared_ptr<network::SocketOptionBuilder> socketOptionBuilder
) : _logger(logger),
    _socketOptionBuilder(socketOptionBuilder),
    _config(config) {
    //
}

Server::Res
Server::Start(std::shared_ptr<network::ISocket> listenSocket) {
    _lifecycle = std::make_unique<network::ServerLifeCycle>(
        _logger,
        _socketOptionBuilder,
        this);

    GUARD(_lifecycle->Start(listenSocket, _config));

    _logger->Info("Server started on port {}.", _config.server.port);
    return Res::Ok();
}

void Server::Stop() {
    if (_lifecycle) {
        _lifecycle->Stop();
        _lifecycle.reset();
    }
}

void Server::OnAccept(std::shared_ptr<network::Client> client) {
    _logger->Info("[Server::OnAccept] Client accepted: socket #{}",
                  client->socket);
}

void Server::OnRecv(std::shared_ptr<network::Client> client,
                    std::span<const char> data) {
    std::string_view recvData{data.data(), data.size()};
    _logger->Info("[Server::OnRecv] Recv: socket #{}, data: {}, bytes: {}",
                  client->socket, recvData, data.size());

    // Echo: 받은 데이터 그대로 전송
    if (!client->PostSend(recvData)) {
        _lifecycle->CloseClient(client, true);
        return;
    }

    // 다음 수신 대기
    if (!client->PostRecv()) {
        _lifecycle->CloseClient(client, true);
    }
}

void Server::OnSend(std::shared_ptr<network::Client> client,
                    size_t bytesTransferred) {
    _logger->Info("[Server::OnSend] Send: socket #{}, bytes: {}", client->socket,
                  bytesTransferred);
}

void Server::OnDisconnect(std::shared_ptr<network::Client> client) {
    _logger->Info("[Server::OnDisconnect] Client disconnected: socket #{}",
                  client->socket);
}
