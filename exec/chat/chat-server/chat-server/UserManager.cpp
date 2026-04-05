#include "UserManager.h"
#include "User.h"
#include "SessionManager.h"

UserManager::UserManager(
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<SessionManager> sessionManager
) : _logger(std::move(logger)),
    _sessionManager(std::move(sessionManager)) {
}

std::shared_ptr<User> UserManager::CreateUser(
    const std::shared_ptr<highp::net::Client>& client,
    std::string_view username,
    uint64_t roomId
) {
    std::shared_ptr<Session> session = _sessionManager->GetSessionByClient(client);
    if (!session) {
        _logger->Error("[UserManager::CreateUser] session not found for socket #{}", client->socket);
        return nullptr;
    }

    if (const User* existingUser = GetUserByClient(client)) {
        _logger->Warn("[UserManager::CreateUser] user already exists for socket #{}, userId={}, roomId={}",
                      client->socket,
                      existingUser->GetId(),
                      existingUser->GetRoomId());
        return nullptr;
    }

    const auto userId = _userIdCounter.fetch_add(1);
    auto user = std::make_shared<User>(
        std::move(session),
        userId,
        username,
        roomId);

    _users[userId] = user;

    return user;
}

void UserManager::RemoveUser(const User& user) {
    this->RemoveUserByUserId(user.GetId());
}

void UserManager::RemoveUserByUserId(uint64_t userId) {
    _users.erase(userId);
}

bool UserManager::IsUserExist(uint64_t userId) const {
    return _users.contains(userId);
}

User* UserManager::GetUser(uint64_t userId) {
    if (const auto it = _users.find(userId); it != _users.end()) {
        return it->second.get();
    }
    return nullptr;
}

size_t UserManager::GetUserCount() const {
    return _users.size();
}

std::vector<const User*> UserManager::GetAllUsers() const {
    std::vector<const User*> users;

    users.reserve(_users.size());
    for (const auto& [_, u] : _users) {
        users.emplace_back(u.get());
    }

    return users;
}

User* UserManager::GetUserByClient(const std::shared_ptr<highp::net::Client>& client) {
    for (const auto& [_, u] : _users) {
        if (u->IsSameClient(client)) {
            return u.get();
        }
    }
    return nullptr;
}
