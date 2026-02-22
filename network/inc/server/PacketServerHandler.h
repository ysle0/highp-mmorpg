#pragma once

#include "IServerHandler.h"

// #include <flatbuf/gen/client_generated.h>
// #include <flatbuf/gen/enum_generated.h>
// #include <flatbuf/gen/packet_generated.h>
// #include <flatbuffers/flatbuffers.h>

namespace highp::log {
    class Logger;
}

namespace highp::net {
    class PacketServerHandler : public IServerHandler {
        // using PacketHandlerType = std::function<void(std::unique_ptr<flatbuffers::Table> packet,
        //                                              std::shared_ptr<Client>)>;

    public:
        explicit PacketServerHandler(
            std::shared_ptr<log::Logger> logger
        );

    private:
        void OnAccept(std::shared_ptr<Client> client) override;
        void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override;
        void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) override;
        void OnDisconnect(std::shared_ptr<Client> client) override;

    protected:
        void MapPackets(std::shared_ptr<Client> client, std::span<const char> data);

    private:
        std::shared_ptr<log::Logger> _logger;
        // flatbuffers::FlatBufferBuilder _builder;
    };
}
