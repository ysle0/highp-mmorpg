#pragma once
#include <atomic>

#include "config/NetworkCfg.h"
#include "server/PacketDispatcher.hpp"
#include "thread/LogicThread.h"

class GameLoop {
    using Res = highp::fn::Result<void, highp::err::ENetworkError>;

public:
    explicit GameLoop(
        std::shared_ptr<highp::log::Logger> logger,
        highp::net::NetworkCfg networkConfig);
    ~GameLoop() noexcept;

public:
    void Start();
    void Stop();
    void Receive(std::shared_ptr<highp::net::Client> client, std::span<const char> data);

private:
    /// Logic thread 메인 루프
    void Update(std::stop_token st);

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::atomic<int> _tickMs;
    highp::net::PacketDispatcher _dispatcher;
    highp::thread::LogicThread _logicThread;
    std::atomic<bool> _hasStopped{false};
};
