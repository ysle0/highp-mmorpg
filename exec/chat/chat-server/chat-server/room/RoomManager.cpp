#include "RoomManager.h"

#include <algorithm>

RoomManager::RoomManager(
    std::shared_ptr<highp::log::Logger> logger,
    uint16_t initRoomCount,
    uint32_t maxRoomCapacity
) : _logger(std::move(logger)),
    _maxRoomCapacity(maxRoomCapacity) {
    _rooms.reserve(initRoomCount);
    for (uint16_t i = 0; i < initRoomCount; ++i) {
        const uint32_t roomId = _roomCounter.fetch_add(1);
        _rooms.emplace_back(std::make_unique<Room>(_logger, roomId));
    }
}

std::shared_ptr<Room> RoomManager::CreateRoom(std::optional<uint32_t> roomIdOverride) {
    std::scoped_lock lock{_mtx};

    const uint32_t roomId = roomIdOverride.has_value()
                                ? roomIdOverride.value()
                                : _roomCounter.fetch_add(1);

    auto room = std::make_shared<Room>(_logger, roomId);
    _rooms.emplace_back(room);

    return room;
}

std::shared_ptr<Room> RoomManager::GetAvailableRoom() {
    std::scoped_lock lock{_mtx};
    for (const std::shared_ptr<Room>& room : _rooms) {
        if (room->GetUserCount() < _maxRoomCapacity) {
            return room;
        }
    }

    const auto roomId = _roomCounter.fetch_add(1);
    auto room = std::make_shared<Room>(_logger, roomId);
    _rooms.emplace_back(room);
    return room;
}

bool RoomManager::DestroyRoom(uint32_t roomId) {
    std::scoped_lock lock{_mtx};
    const size_t oldSize = _rooms.size();
    std::erase_if(_rooms, [roomId](const std::shared_ptr<Room>& room) {
        return room->GetId() == roomId;
    });
    return oldSize != _rooms.size();
}

bool RoomManager::IsRoomExist(uint32_t roomId) const {
    std::scoped_lock lock{_mtx};
    return std::any_of(
        _rooms.begin(), _rooms.end(),
        [roomId](const std::shared_ptr<Room>& room) {
            return room->GetId() == roomId;
        });
}

Room* RoomManager::GetRoom(uint32_t roomId) {
    std::scoped_lock lock{_mtx};
    const auto it = std::find_if(
        _rooms.begin(),
        _rooms.end(),
        [roomId](const std::shared_ptr<Room>& room) {
            return room->GetId() == roomId;
        });
    return it == _rooms.end() ? nullptr : it->get();
}

void RoomManager::KickDisconnected(const std::shared_ptr<highp::net::Client>& client) {
    std::scoped_lock lock{_mtx};
    for (const std::shared_ptr<Room>& room : _rooms) {
        room->KickByDisconnected(client);
    }
}
