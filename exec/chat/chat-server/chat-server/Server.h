#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "GameLoop.h"
#include "config/NetworkCfg.h"
#include "logger/Logger.hpp"
#include "server/ISessionEventReceiver.h"
#include "server/ServerLifecycle.h"
#include "socket/ISocket.h"
#include "socket/SocketOptionBuilder.h"

class Server final : public highp::net::ISessionEventReceiver {
    using Res = highp::fn::Result<void, highp::err::ENetworkError>;

public:
    struct EventCallbacks {
        highp::net::ServerLifeCycle::EventCallbacks lifecycle;
        std::function<void()> onStarted;
        std::function<void()> onStopping;
    };

    explicit Server(
        std::shared_ptr<highp::log::Logger> logger,
        std::unique_ptr<GameLoop> gameLoop,
        highp::net::NetworkCfg networkCfg,
        std::shared_ptr<highp::net::SocketOptionBuilder> socketOptionBuilder);
    ~Server() noexcept override;

    void UseCallbacks(EventCallbacks callbacks) noexcept;
    Res Start(std::shared_ptr<highp::net::ISocket> listenSocket);
    void Stop();

private:
    void OnAccept(std::shared_ptr<highp::net::Client> client) override;
    void OnRecv(std::shared_ptr<highp::net::Client> client, std::span<const char> data) override;
    void OnSend(std::shared_ptr<highp::net::Client> client, size_t bytesTransferred) override;
    void OnDisconnect(std::shared_ptr<highp::net::Client> client) override;

    /// Logic thread 메인 루프
    void LogicLoop(std::stop_token st);

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<highp::net::SocketOptionBuilder> _socketOptionBuilder;
    highp::net::NetworkCfg _config;
    std::unique_ptr<highp::net::ServerLifeCycle> _lifecycle;
    std::atomic<int> _tickMs;

    std::atomic<bool> _hasStopped;
    std::unique_ptr<GameLoop> _gameLoop;
    EventCallbacks _callbacks;
};
