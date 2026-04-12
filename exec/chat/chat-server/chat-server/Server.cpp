#include "Server.h"
#include "SelfHandlerRegistry.h"

#include <scope/Defer.h>
#include <utility>

Server::Server(
    std::shared_ptr<highp::log::Logger> logger,
    std::unique_ptr<GameLoop> gameLoop,
    highp::net::NetworkCfg networkCfg,
    std::shared_ptr<highp::net::SocketOptionBuilder> socketOptionBuilder
) : _logger(std::move(logger)),
    _gameLoop(std::move(gameLoop)),
    _config(networkCfg),
    _socketOptionBuilder(std::move(socketOptionBuilder)) {
}

Server::~Server() noexcept {
    if (!_hasStopped.load()) {
        Stop();
    }
}

void Server::UseCallbacks(EventCallbacks callbacks) noexcept {
    _callbacks = std::move(callbacks);
}

Server::Res Server::Start(std::shared_ptr<highp::net::ISocket> listenSocket) {
    _lifecycle = std::make_unique<highp::net::ServerLifeCycle>(
        _logger,
        _socketOptionBuilder,
        this);
    _lifecycle->UseCallbacks(_callbacks.lifecycle);

    if (const Res lifecycleStartRes = _lifecycle->Start(listenSocket, _config);
        lifecycleStartRes.HasErr()) {
        return lifecycleStartRes;
    }

    if (_callbacks.onStarted) {
        _callbacks.onStarted();
    }

    _logger->Info("Server started on port {}.", _config.server.port);
    _gameLoop->Start();
    return Res::Ok();
}

void Server::Stop() {
    DEFER([this] {
        _hasStopped.store(true);
        });

    if (_lifecycle) {
        _lifecycle->Stop();
        _lifecycle.reset();
    }

    if (_gameLoop) {
        _gameLoop->Stop();
        _gameLoop.reset();
    }

    if (_callbacks.onStopping) {
        _callbacks.onStopping();
    }
}

void Server::OnAccept(std::shared_ptr<highp::net::Client> client) {
    _logger->Debug("[Server::OnAccept]: socket #{}", client->socket);
    _gameLoop->Connect(client);
}

void Server::OnRecv(std::shared_ptr<highp::net::Client> client, std::span<const char> data) {
    _logger->Debug("[Server::OnRecv]: socket #{}, data length: {}",
                   client->socket,
                   data.size());
    _gameLoop->Receive(client, data);
}

void Server::OnSend(std::shared_ptr<highp::net::Client> client, size_t bytesTransferred) {
    _logger->Debug("[Server::OnSend]: socket #{}, bytes: {}",
                   client->socket, bytesTransferred);
}

void Server::OnDisconnect(std::shared_ptr<highp::net::Client> client) {
    _logger->Debug("[Server::OnDisconnect]: socket #{}", client->socket);
    _gameLoop->Disconnect(client);
}
