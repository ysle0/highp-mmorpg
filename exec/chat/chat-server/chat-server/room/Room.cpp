#include "Room.h"

#include "../User.h"
#include "PacketFactory.h"
#include "time/time.h"

void Room::Join(std::unique_ptr<User> user) {
    {
        std::scoped_lock lock{_mtx};
        _users.emplace_back(std::move(user));
    }
}

void Room::Leave(uint32_t userId) {
    {
        std::scoped_lock lock{_mtx};
        erase_if(_users, [userId](auto& user) {
            return user->GetId() == userId;
        });
    }
}

void Room::BroadcastUserJoined(uint32_t userId, std::string_view userName) {
    {
        std::scoped_lock lock{_mtx};
        const auto pkt = highp::protocol::MakeUserJoinedBroadcast(userId, userName);
        for (const auto& u : _users) {
            u->Send(pkt);
        }
    }
}

void Room::BroadcastUserLeft(uint32_t userId, std::string_view userName) {
    {
        std::scoped_lock lock{_mtx};
        const auto pkt = highp::protocol::MakeUserLeftBroadcast(userId, userName);
        for (const auto& u : _users) {
            u->Send(pkt);
        }
    }
}

void Room::BroadcastChatMessage(std::string_view chatMessage) {
    {
        std::scoped_lock lock{_mtx};
        const auto now = highp::protocol::now();
        const auto pkt = highp::protocol::MakeChatMessageBroadcast(0, chatMessage, now);
        for (const auto& u : _users) {
            u->Send(pkt);
        }
    }
}
