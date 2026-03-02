#pragma once
#include <memory>
#include <client/PacketStream.h>
#include <client/TcpClientSocket.h>
#include <client/WsaSession.h>

#include "../../../protocol/flatbuf/gen/broadcast_generated.h"

namespace highp::log {
    class Logger;
}

using namespace highp;

class Client {
public:
    explicit Client(std::shared_ptr<log::Logger> logger);

    ~Client() noexcept;

    [[nodiscard]] bool Connect(const char* ipAddress, unsigned short port);

    [[nodiscard]] bool Disconnect();

    void Send(const flatbuffers::FlatBufferBuilder& builder);

private:
    std::shared_ptr<log::Logger> _logger;

    std::shared_ptr<net::WsaSession> _wsaSession;

    std::unique_ptr<net::TcpClientSocket> _tcpClientSocket;

    std::unique_ptr<net::PacketStream> _packetStream;
};
