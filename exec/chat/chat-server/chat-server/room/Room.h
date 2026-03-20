#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

class User;

class Room {
public:
    void Join(std::unique_ptr<User> user);
    void Leave(uint32_t userId);
    void BroadcastUserJoined(uint32_t userId, std::string_view userName);
    void BroadcastUserLeft(uint32_t userId, std::string_view userName);
    void BroadcastChatMessage(std::string_view chatMessage);

private:
    std::mutex _mtx;
    std::vector<std::unique_ptr<User>> _users;
};
