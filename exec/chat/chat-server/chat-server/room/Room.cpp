#include "Room.h"

#include <algorithm>

#include "PacketFactory.h"
#include "time/Time.h"

Room::Room(
    std::shared_ptr<highp::log::Logger> logger,
    uint32_t roomId
) :
    _logger(std::move(logger)),
    _roomId(roomId) {
}

void Room::Join(const std::shared_ptr<User>& user) {
    if (!user) {
        _logger->Warn("[Room #{}] user is nullptr]", _roomId);
        return;
    }

    this->BroadcastUserJoined(
        static_cast<uint32_t>(user->GetId()),
        user->GetUsername());

    std::scoped_lock lock{_mtx};
    _users.emplace_back(user);
}

void Room::Leave(uint64_t userId) {
    std::string_view username;
    {
        std::scoped_lock lock{_mtx};
        if (_users.empty()) {
            _logger->Warn("[Room #{}] user list is empty]", _roomId);
            return;
        }

        const auto found = std::find_if(
            _users.begin(),
            _users.end(),
            [userId](const std::shared_ptr<User>& user) {
                return user->GetId() == userId;
            });
        if (found == _users.end()) {
            _logger->Warn("[Room #{}] user not found in user list]", _roomId);
            return;
        }

        username = (*found)->GetUsername();
        _users.erase(found);
    }
    this->BroadcastUserLeft(static_cast<uint32_t>(userId), username);
}

void Room::BroadcastUserJoined(uint32_t userId, std::string_view userName) {
    std::scoped_lock lock{_mtx};
    const auto pkt = highp::protocol::MakeUserJoinedBroadcast(userId, userName);
    for (const auto& u : _users) {
        if (u->GetId() == userId) {
            continue;
        }

        u->Send(pkt);
    }
}

void Room::BroadcastUserLeft(uint32_t userId, std::string_view userName) {
    std::scoped_lock lock{_mtx};
    const auto pkt = highp::protocol::MakeUserLeftBroadcast(userId, userName);
    for (const auto& u : _users) {
        if (u->GetId() == userId) {
            continue;
        }

        u->Send(pkt);
    }
}

void Room::BroadcastChatMessage(uint64_t userId, std::string_view chatMessage) {
    std::scoped_lock lock{_mtx};
    const auto now = highp::protocol::now();
    const auto pkt = highp::protocol::MakeChatMessageBroadcast(0, chatMessage, now);
    for (const auto& u : _users) {
        if (u->GetId() == userId) {
            continue;
        }

        u->Send(pkt);
    }
}

size_t Room::GetUserCount() const {
    std::scoped_lock lock{_mtx};
    return _users.size();
}

void Room::Kick(uint64_t userId) {
    std::scoped_lock lock{_mtx};
    std::erase_if(_users, [userId](const std::shared_ptr<User>& user) {
        return user->GetId() == userId;
    });
}

void Room::KickByDisconnected(const std::shared_ptr<highp::net::Client>& client) {
    std::scoped_lock lock{_mtx};
    std::erase_if(_users, [&client](const std::shared_ptr<User>& user) {
        return user->IsSameClient(client);
    });
}
