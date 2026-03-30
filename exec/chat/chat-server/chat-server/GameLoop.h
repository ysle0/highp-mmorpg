#pragma once
#include <atomic>

#include "SessionManager.h"
#include "UserManager.h"
#include "config/NetworkCfg.h"
#include "room/RoomManager.h"
#include "server/PacketDispatcher.hpp"
#include "thread/LogicThread.h"

class GameLoop final {
    using Res = highp::fn::Result<void, highp::err::ENetworkError>;

public:
    explicit GameLoop(
        std::shared_ptr<highp::log::Logger> logger,
        std::unique_ptr<highp::net::PacketDispatcher> packetDispatcher,
        std::shared_ptr<RoomManager> roomManager,
        std::shared_ptr<SessionManager> sessionManager,
        std::shared_ptr<UserManager> userManager,
        highp::net::NetworkCfg networkConfig
    );
    ~GameLoop() noexcept;

public:
    void Connect(const std::shared_ptr<highp::net::Client>& client);
    void Start();
    void Stop();
    void Receive(const std::shared_ptr<highp::net::Client>& client, std::span<const char> data) const;
    void Disconnect(const std::shared_ptr<highp::net::Client>& client);

private:
    /// Logic thread 메인 루프
    void Update(std::stop_token st) const;

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::atomic<int> _tickMs;
    std::unique_ptr<highp::net::PacketDispatcher> _dispatcher;
    highp::thread::LogicThread _logicThread;
    std::atomic<bool> _hasStopped{false};
    std::shared_ptr<RoomManager> _roomManager;
    std::shared_ptr<SessionManager> _sessionManager;
    std::shared_ptr<UserManager> _userManager;
};
