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

    _logicThread = std::jthread([this](std::stop_token st) {
        LogicLoop(st);
    });

    _logger->Info("Server started on port {}.", _config.server.port);
    return Res::Ok();
}

void Server::Stop() {
    scope::Defer _([this] {
        _hasStopped.store(true);
    });

    // logic thread 먼저 종료
    if (_logicThread.joinable()) {
        _logicThread.request_stop();
        // Tick() 의 sleep_until 은 즉시 깨울 수 없어 tickMs 정도 기다릴 수 있음.
        _logicThread.join();
    }

    if (_lifecycle) {
        _lifecycle->Stop();
        _lifecycle.reset();
    }
}

void Server::LogicLoop(std::stop_token st) {
    _logger->Info("[LogicThread] started. ServerTick={}ms", _tickMs.load());

    while (!st.stop_requested()) {
        const auto nextTick = std::chrono::steady_clock::now() + std::chrono::milliseconds(_tickMs.load());
        _dispatcher.Tick();
        std::this_thread::sleep_until(nextTick);
    }

    // 종료 전 잔여 커맨드 처리
    _dispatcher.Tick();
    _logger->Info("[LogicThread] stopped.");
}

void Server::OnAccept(std::shared_ptr<net::Client> client) {
    _logger->Info("[Server::OnAccept]: socket #{}", client->socket);
}

void Server::OnRecv(std::shared_ptr<net::Client> client, std::span<const char> data) {
    _dispatcher.Receive(client, data);
}

void Server::OnSend(std::shared_ptr<net::Client> client, size_t bytesTransferred) {
    _logger->Info("[Server::OnSend]: socket #{}, bytes: {}", client->socket, bytesTransferred);
}

void Server::OnDisconnect(std::shared_ptr<net::Client> client) {
    _logger->Info("[Server::OnDisconnect]: socket #{}", client->socket);
}
