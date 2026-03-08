#pragma once
#include <atomic>

#include "config/NetworkCfg.h"
#include "server/PacketDispatcher.hpp"
#include "thread/LogicThread.h"

using namespace highp;

class GameLoop {
    using Res = fn::Result<void, err::ENetworkError>;

public:
    explicit GameLoop(
        std::shared_ptr<log::Logger> logger,
        net::NetworkCfg networkConfig);
    ~GameLoop() noexcept;

public:
    void Start();
    void Stop();
    void Receive(std::shared_ptr<net::Client> shared, std::span<const char> span);

private:
    /// Logic thread 메인 루프
    void Update(std::stop_token st);

private:
    std::shared_ptr<log::Logger> _logger;
    std::atomic<int> _tickMs;
    net::PacketDispatcher _dispatcher;
    thread::LogicThread _logicThread;
    std::atomic<bool> _hasStopped;
};
