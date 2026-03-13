#pragma once
#include <server/IPacketHandler.hpp>
#include <flatbuf/gen/packet_generated.h>
#include <logger/Logger.hpp>

class ChatMessageHandler
    : public highp::net::IPacketHandler<highp::protocol::messages::ChatMessageBroadcast> {
public:
    explicit ChatMessageHandler(std::shared_ptr<highp::log::Logger> logger);

    void Handle(
        std::shared_ptr<highp::net::Client> client,
        const highp::protocol::messages::ChatMessageBroadcast* payload
    ) override;

private:
    std::shared_ptr<highp::log::Logger> _logger;
};
