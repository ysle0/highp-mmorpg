#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include "client/windows/Client.h"
#include "../User.h"

class Room {
public:
    explicit Room(uint32_t roomId);

    // packet handle logics
public:
    void Join(std::unique_ptr<User> user);
    void Leave(uint32_t userId);
    void BroadcastUserJoined(uint32_t userId, std::string_view userName);
    void BroadcastUserLeft(uint32_t userId, std::string_view userName);
    void BroadcastChatMessage(std::string_view chatMessage);

public:
    void Kick(uint32_t userId);
    void KickByDisconnected(const std::shared_ptr<highp::net::Client>& client);

public:
    [[nodiscard]] uint32_t GetId() const { return _roomId; }

private:
    uint32_t _roomId{0};

    std::mutex _mtx;
    std::vector<std::unique_ptr<User>> _users;
};
