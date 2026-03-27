#pragma once
#include <server/IPacketHandler.hpp>
#include <flatbuf/gen/packet_generated.h>
#include <logger/Logger.hpp>

class LeaveRoomHandler
    : public highp::net::IPacketHandler<highp::protocol::messages::LeaveRoomRequest> {
public:
    explicit LeaveRoomHandler(std::shared_ptr<highp::log::Logger> logger);

    void Handle(
        std::shared_ptr<highp::net::Client> client,
        const highp::protocol::messages::LeaveRoomRequest* payload
    ) override;

private:
    std::shared_ptr<highp::log::Logger> _logger;
};
