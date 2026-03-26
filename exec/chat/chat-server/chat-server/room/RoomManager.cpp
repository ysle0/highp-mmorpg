#include "RoomManager.h"

#include <algorithm>

RoomManager::RoomManager(
    std::shared_ptr<highp::log::Logger> logger,
    uint16_t initRoomCount
) : _logger(std::move(logger)) {
    _rooms.reserve(initRoomCount);
    for (uint16_t i = 0; i < initRoomCount; ++i) {
        const auto roomId = _roomCounter.fetch_add(1);
        _rooms.emplace_back(std::make_unique<Room>(roomId));
    }
}

void RoomManager::CreateRoom(std::optional<uint32_t> roomIdOverride) {
    std::scoped_lock lock{_mtx};
    const uint32_t roomId = roomIdOverride.has_value()
                                ? roomIdOverride.value()
                                : _roomCounter.fetch_add(1);
    _rooms.emplace_back(std::make_unique<Room>(roomId));
}

bool RoomManager::DestroyRoom(uint32_t roomId) {
    std::scoped_lock lock{_mtx};
    const auto oldSize = _rooms.size();
    std::erase_if(_rooms, [roomId](const std::unique_ptr<Room>& room) {
        return room->GetId() == roomId;
    });
    return oldSize != _rooms.size();
}

bool RoomManager::IsRoomExist(uint32_t roomId) const {
    std::scoped_lock lock{_mtx};
    return std::any_of(
        _rooms.begin(), _rooms.end(),
        [roomId](const std::unique_ptr<Room>& room) {
            return room->GetId() == roomId;
        });
}

Room* RoomManager::GetRoom(uint32_t roomId) {
    std::scoped_lock lock{_mtx};
    const auto it = std::find_if(
        _rooms.begin(),
        _rooms.end(),
        [roomId](const std::unique_ptr<Room>& room) {
            return room->GetId() == roomId;
        });
    return it == _rooms.end() ? nullptr : it->get();
}

void RoomManager::KickDisconnectedClient(const std::shared_ptr<highp::net::Client>& client) {
    std::scoped_lock lock{_mtx};
    for (const auto& room : _rooms) {
        room->KickByDisconnected(client);
    }
}
