#include "Room.h"

#include <algorithm>

#include "PacketFactory.h"
#include "time/Time.h"

Room::Room(
    std::shared_ptr<highp::log::Logger> logger,
    uint64_t roomId
) :
    _logger(std::move(logger)),
    _roomId(roomId) {
}

void Room::Join(const std::shared_ptr<User>& user) {
    if (!user) {
        _logger->Warn("[Room #{}] user is nullptr]", _roomId);
        return;
    }

    const uint64_t userId = user->GetId();
    const std::string_view userName = user->GetUsername();

    std::scoped_lock lock{_mtx};

    const flatbuffers::FlatBufferBuilder pkt =
        highp::protocol::makeUserJoinedBroadcast(_roomId, userId, userName);

    for (const std::shared_ptr<User>& u : _users) {
        u->Send(pkt);
    }

    _users.emplace_back(user);
}

void Room::Leave(uint64_t userId) {
    std::scoped_lock lock{_mtx};
    if (_users.empty()) {
        _logger->Warn("[Room #{}] user list is empty]", _roomId);
        return;
    }

    const auto found = std::find_if(
        _users.begin(), _users.end(),
        [userId](const std::shared_ptr<User>& user) {
            return user->GetId() == userId;
        });

    if (found == _users.end()) {
        _logger->Warn("[Room #{}] user not found in user list]", _roomId);
        return;
    }

    const std::string username{(*found)->GetUsername()};
    _users.erase(found);

    const flatbuffers::FlatBufferBuilder pkt =
        highp::protocol::makeUserLeftBroadcast(userId, username);

    for (const std::shared_ptr<User>& u : _users) {
        if (u->GetId() == userId) {
            continue;
        }

        u->Send(pkt);
    }
}

void Room::BroadcastChatMessage(
    uint32_t senderId,
    uint64_t userId,
    std::string_view username,
    std::string_view chatMessage
) {
    std::scoped_lock lock{_mtx};

    const highp::protocol::Common::Timestamp now = highp::protocol::now();
    const flatbuffers::FlatBufferBuilder pkt = highp::protocol::makeChatMessageBroadcast(
        senderId, username, chatMessage, now);

    for (const std::shared_ptr<User>& u : _users) {
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
