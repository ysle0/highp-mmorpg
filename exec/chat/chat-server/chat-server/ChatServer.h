#pragma once

#include <server/PacketServerHandler.h>

namespace highp::log {
    class Logger;
}

namespace highp::net {
    class NetworkCfg;
    class SocketOptionBuilder;
    class ISocket;
    class Client;
}

class ChatServerHandler final
    : public highp::net::PacketServerHandler {
public:
    explicit ChatServerHandler(
        std::shared_ptr<highp::log::Logger> logger,
        highp::net::NetworkCfg networkCfg,
        std::shared_ptr<highp::net::SocketOptionBuilder> socketOptBuilder
    );

    ~ChatServerHandler() noexcept override;

    highp::fn::Result<void, highp::err::ENetworkError> Start(
        std::shared_ptr<highp::net::ISocket> listenSocket);

    void Stop();
};
