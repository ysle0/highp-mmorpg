#include "Server.h"
#include "SelfHandlerRegistry.h"

#include <scope/Defer.h>
#include <chrono>

Server::Server(
    std::shared_ptr<log::Logger> logger,
    net::NetworkCfg networkCfg,
    std::shared_ptr<net::SocketOptionBuilder> socketOptionBuilder)
    : _logger(logger),
      _socketOptionBuilder(socketOptionBuilder),
      _config(networkCfg),
      _dispatcher(logger) {
    _tickMs.store(networkCfg.server.tickMs);
    SelfHandlerRegistry::Instance().RegisterAll(_dispatcher, _logger);
}

Server::~Server() noexcept {
    if (!_hasStopped.load()) {
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
    _gameLoop->Start();
    return Res::Ok();
}

void Server::Stop() {
    DEFER([this] {
        _gameLoop->Stop();
        _hasStopped.store(true);
    });

    if (_lifecycle) {
        _lifecycle->Stop();
        _lifecycle.reset();
    }
}


void Server::OnAccept(std::shared_ptr<net::Client> client) {
    _logger->Debug("[Server::OnAccept]: socket #{}", client->socket);
}

void Server::OnRecv(std::shared_ptr<net::Client> client, std::span<const char> data) {
    _logger->Debug("[Server::OnRecv]: socket #{}, data: {}", client->socket, data);
    _gameLoop->Receive(client, data);
}

void Server::OnSend(std::shared_ptr<net::Client> client, size_t bytesTransferred) {
    _logger->Debug("[Server::OnSend]: socket #{}, bytes: {}", client->socket, bytesTransferred);
}

void Server::OnDisconnect(std::shared_ptr<net::Client> client) {
    _logger->Debug("[Server::OnDisconnect]: socket #{}", client->socket);
    _gameLoop->Stop();
}
