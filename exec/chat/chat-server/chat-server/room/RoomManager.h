#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "logger/Logger.hpp"
#include "Room.h"

class RoomManager final {
public:
    explicit RoomManager(
        std::shared_ptr<highp::log::Logger> logger,
        uint16_t initRoomCount,
        uint32_t maxRoomCapacity
    );

public:
    std::shared_ptr<Room> CreateRoom(std::optional<uint32_t> roomIdOverride = std::nullopt);
    std::shared_ptr<Room> GetOrCreateAvailableRoom();
    bool DestroyRoom(uint32_t roomId);
    bool IsRoomExist(uint32_t roomId) const;
    Room* GetRoom(uint32_t roomId);
    void KickDisconnected(const std::shared_ptr<highp::net::Client>& client);

private:
    std::shared_ptr<highp::log::Logger> _logger;

    mutable std::mutex _mtx;
    std::vector<std::shared_ptr<Room>> _rooms;
    std::atomic<uint32_t> _roomCounter{0};
    uint32_t _maxRoomCapacity;
};
