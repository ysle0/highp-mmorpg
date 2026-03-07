#include "Server.h"

Server::Server(
    std::shared_ptr<log::Logger> logger,
    net::NetworkCfg networkCfg,
    std::shared_ptr<net::SocketOptionBuilder> socketOptionBuilder)
    : PacketHandler(logger->WithPrefix("[ChatServer] ")),
      _socketOptionBuilder(socketOptionBuilder),
      _config(networkCfg) {
    //
}

Server::~Server() noexcept {
    if (_lifecycle) {
        _lifecycle->Stop();
        _lifecycle.reset();
    }
}

Server::Res Server::Start(std::shared_ptr<net::ISocket> listenSocket) {
    _lifecycle = std::make_unique<net::ServerLifeCycle>(
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

void Server::OnRecv(std::shared_ptr<net::Client> client, std::span<const char> data) {
    _logger->Info("[Server::OnRecv]: socket #{}, data: {}, bytes: {}", client->socket, data.data(), data.size());
    PacketHandler::OnRecv(client, data);
}

void Server::OnSend(std::shared_ptr<net::Client> client, size_t bytesTransferred) {
    _logger->Info("[Server::OnSend]: socket #{}, bytes: {}", client->socket, bytesTransferred);
}

void Server::OnDisconnect(std::shared_ptr<net::Client> client) {
    _logger->Info("[Server::OnDisconnect]: socket #{}", client->socket);
}
