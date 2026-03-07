#pragma once
#include <server/IPacketHandler.hpp>
#include <flatbuf/gen/packet_generated.h>
#include <logger/Logger.hpp>

using namespace highp;

class JoinRoomHandler : public net::IPacketHandler<protocol::messages::JoinRoomRequest> {
public:
    explicit JoinRoomHandler(std::shared_ptr<log::Logger> logger);

    void Handle(
        std::shared_ptr<net::Client> client,
        const protocol::messages::JoinRoomRequest* payload
    ) override;

private:
    std::shared_ptr<log::Logger> _logger;
};
