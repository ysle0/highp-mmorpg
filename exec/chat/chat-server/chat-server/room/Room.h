#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include "client/windows/Client.h"
#include "logger/Logger.hpp"
#include "../User.h"

class Room {
public:
    explicit Room(
        std::shared_ptr<highp::log::Logger> logger,
        uint32_t roomId);

    // packet handle logics
public:
    void Join(const std::shared_ptr<User>& user);
    void Leave(uint64_t userId);
    void BroadcastUserJoined(uint64_t userId, std::string_view userName);
    void BroadcastUserLeft(uint64_t userId, std::string_view userName);
    void BroadcastChatMessage(std::string_view chatMessage);

public:
    void Kick(uint64_t userId);
    void KickByDisconnected(const std::shared_ptr<highp::net::Client>& client);

public:
    [[nodiscard]] uint32_t GetId() const { return _roomId; }
    [[nodiscard]] size_t GetUserCount() const;

private:
    std::shared_ptr<highp::log::Logger> _logger;

    std::mutex _mtx;
    std::vector<std::shared_ptr<User>> _users;

    uint32_t _roomId{0};
};
