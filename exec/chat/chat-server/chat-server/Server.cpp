#include "Server.h"

#include <scope/Defer.hpp>

Server::Server(
    std::shared_ptr<log::Logger> logger,
    net::NetworkCfg networkCfg,
    std::shared_ptr<net::SocketOptionBuilder> socketOptionBuilder)
    : _logger(logger),
      _socketOptionBuilder(socketOptionBuilder),
      _config(networkCfg),
      _dispatcher(logger),
      _chatMessageHandler(logger),
      _joinRoomHandler(logger) {

    _dispatcher.RegisterHandler<protocol::messages::ChatMessageBroadcast>(&_chatMessageHandler);
    _dispatcher.RegisterHandler<protocol::messages::JoinRoomRequest>(&_joinRoomHandler);
}

Server::~Server() noexcept {
    if (!_hasStopped) {
        Stop();
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
    scope::Defer _([this] {
        _hasStopped = true;
    });

    if (_lifecycle) {
        _lifecycle->Stop();
        _lifecycle.reset();
    }
}

void Server::OnAccept(std::shared_ptr<net::Client> client) {
    _logger->Info("[Server::OnAccept]: socket #{}", client->socket);
}

void Server::OnRecv(std::shared_ptr<net::Client> client, std::span<const char> data) {
    _dispatcher.Handle(client, data);
}

void Server::OnSend(std::shared_ptr<net::Client> client, size_t bytesTransferred) {
    _logger->Info("[Server::OnSend]: socket #{}, bytes: {}", client->socket, bytesTransferred);
}

void Server::OnDisconnect(std::shared_ptr<net::Client> client) {
    _logger->Info("[Server::OnDisconnect]: socket #{}", client->socket);
}
