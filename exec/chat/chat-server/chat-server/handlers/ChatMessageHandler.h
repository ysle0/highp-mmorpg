#pragma once
#include <server/IPacketHandler.hpp>
#include <flatbuf/gen/packet_generated.h>
#include <logger/Logger.hpp>

using namespace highp;

class ChatMessageHandler : public net::IPacketHandler<protocol::messages::ChatMessageBroadcast> {
public:
    explicit ChatMessageHandler(std::shared_ptr<log::Logger> logger);

    void Handle(
        std::shared_ptr<net::Client> client,
        const protocol::messages::ChatMessageBroadcast* payload
    ) override;

private:
    std::shared_ptr<log::Logger> _logger;
};
