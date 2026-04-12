#pragma once

#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <span>

#include "SessionManager.h"
#include "UserManager.h"
#include "config/NetworkCfg.h"
#include "logger/Logger.hpp"
#include "platform.h"
#include "room/RoomManager.h"
#include "server/PacketDispatcher.hpp"
#include "thread/LogicThread.h"

class GameLoop final {
    using Res = highp::fn::Result<void, highp::err::ENetworkError>;
    using LogicThreadStartedCallback = std::function<void(DWORD)>;

public:
    struct EventCallbacks {
        highp::net::PacketDispatcherCallbacks dispatcher;
        std::function<void(std::chrono::nanoseconds)> onTickDuration;
        std::function<void(std::chrono::nanoseconds)> onTickLag;
        LogicThreadStartedCallback onLogicThreadStarted;
    };

    explicit GameLoop(
        std::shared_ptr<highp::log::Logger> logger,
        std::unique_ptr<highp::net::PacketDispatcher> packetDispatcher,
        std::shared_ptr<RoomManager> roomManager,
        std::shared_ptr<SessionManager> sessionManager,
        std::shared_ptr<UserManager> userManager,
        const highp::net::NetworkCfg& networkConfig);
    ~GameLoop() noexcept;

public:
    void UseCallbacks(EventCallbacks callbacks) noexcept;
    void Connect(const std::shared_ptr<highp::net::Client>& client);
    void Start();
    void Stop();
    void Receive(const std::shared_ptr<highp::net::Client>& client, std::span<const char> data) const;
    void Disconnect(const std::shared_ptr<highp::net::Client>& client);

private:
    void Update(std::stop_token st);

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::atomic<int> _tickMs;
    std::unique_ptr<highp::net::PacketDispatcher> _dispatcher;
    highp::thread::LogicThread _logicThread;
    std::atomic<bool> _hasStopped{false};
    std::shared_ptr<RoomManager> _roomManager;
    std::shared_ptr<SessionManager> _sessionManager;
    std::shared_ptr<UserManager> _userManager;
    EventCallbacks _callbacks;
};
