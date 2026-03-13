#pragma once
#include <server/IPacketHandler.hpp>
#include <flatbuf/gen/packet_generated.h>
#include <logger/Logger.hpp>

using namespace highp;

class LeaveRoomHandler : public net::IPacketHandler<protocol::messages::LeaveRoomRequest> {
public:
    explicit LeaveRoomHandler(std::shared_ptr<log::Logger> logger);

    void Handle(
        std::shared_ptr<net::Client> client,
        const protocol::messages::LeaveRoomRequest* payload
    ) override;

private:
    std::shared_ptr<log::Logger> _logger;
};
