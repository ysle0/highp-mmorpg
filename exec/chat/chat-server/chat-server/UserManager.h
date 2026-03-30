#pragma once
#include <atomic>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "logger/Logger.hpp"

namespace highp::net {
    struct Client;
}

class SessionManager;
class User;

class UserManager {
public:
    explicit UserManager(
        std::shared_ptr<highp::log::Logger> logger,
        std::shared_ptr<SessionManager> sessionManager);

    std::shared_ptr<User> CreateUser(
        const std::shared_ptr<highp::net::Client>& client,
        std::string_view username,
        uint32_t roomId);

    void RemoveUser(const User& user);
    void RemoveUserByUserId(uint64_t userId);

    [[nodiscard]] bool IsUserExist(uint64_t userId) const;
    [[nodiscard]] User* GetUser(uint64_t userId);
    [[nodiscard]] size_t GetUserCount() const;
    [[nodiscard]] std::vector<const User*> GetAllUsers() const;
    [[nodiscard]] User* GetUserByClient(const std::shared_ptr<highp::net::Client>& client);

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<SessionManager> _sessionManager;

    std::atomic<uint64_t> _userIdCounter{0};
    std::unordered_map<uint64_t, std::shared_ptr<User>> _users;
};
