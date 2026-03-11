#pragma once
#include <server/IPacketHandler.hpp>
#include <flatbuf/gen/packet_generated.h>
#include <logger/Logger.hpp>

class JoinRoomHandler
    : public highp::net::IPacketHandler<highp::protocol::messages::JoinRoomRequest> {
public:
    explicit JoinRoomHandler(std::shared_ptr<highp::log::Logger> logger);

    void Handle(
        std::shared_ptr<highp::net::Client> client,
        const highp::protocol::messages::JoinRoomRequest* payload
    ) override;

private:
    std::shared_ptr<highp::log::Logger> _logger;
};
