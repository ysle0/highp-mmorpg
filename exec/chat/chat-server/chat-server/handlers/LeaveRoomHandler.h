#pragma once
#include <server/IPacketHandler.hpp>
#include <flatbuf/gen/packet_generated.h>
#include <logger/Logger.hpp>

#include "../UserManager.h"
#include "../room/RoomManager.h"

class LeaveRoomHandler
    : public highp::net::IPacketHandler<highp::protocol::messages::LeaveRoomRequest> {
public:
    explicit LeaveRoomHandler(
        std::shared_ptr<highp::log::Logger> logger,
        std::shared_ptr<RoomManager> roomManager,
        std::shared_ptr<UserManager> userManager);

    void Handle(
        const std::shared_ptr<highp::net::Client>& client,
        const highp::protocol::Packet* packet,
        const highp::protocol::messages::LeaveRoomRequest* payload
    ) override;

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<RoomManager> _roomManager;
    std::shared_ptr<UserManager> _userManager;
};
