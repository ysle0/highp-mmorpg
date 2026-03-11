#pragma once
#include <atomic>
#include <memory>

#include "GameLoop.h"
#include "config/NetworkCfg.h"
#include "server/ISessionEventReceiver.h"
#include "server/ServerLifecycle.h"
#include "socket/ISocket.h"
#include "socket/SocketOptionBuilder.h"

#include "handlers/ChatMessageHandler.h"
#include "handlers/JoinRoomHandler.h"

class Server : public highp::net::ISessionEventReceiver {
    using Res = highp::fn::Result<void, highp::err::ENetworkError>;

public:
    explicit Server(
        std::shared_ptr<highp::log::Logger> logger,
        highp::net::NetworkCfg networkCfg,
        std::shared_ptr<highp::net::SocketOptionBuilder> socketOptionBuilder = nullptr);
    ~Server() noexcept override;

    Res Start(std::shared_ptr<highp::net::ISocket> listenSocket);
    void Stop();

private:
    void OnAccept(std::shared_ptr<highp::net::Client> client) override;
    void OnRecv(std::shared_ptr<highp::net::Client> client, std::span<const char> data) override;
    void OnSend(std::shared_ptr<highp::net::Client> client, size_t bytesTransferred) override;
    void OnDisconnect(std::shared_ptr<highp::net::Client> client) override;

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<highp::net::SocketOptionBuilder> _socketOptionBuilder;
    highp::net::NetworkCfg _config;
    std::atomic<bool> _hasStopped{false};

    std::unique_ptr<highp::net::ServerLifeCycle> _lifecycle;
    std::unique_ptr<GameLoop> _gameLoop;
};
