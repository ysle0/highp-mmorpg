#pragma once

#include <memory>
#include <client/PacketStream.h>
#include <client/TcpClientSocket.h>
#include <client/WsaSession.h>
#include <flatbuf/gen/broadcast_generated.h>
#include <flatbuffers/flatbuffer_builder.h>
#include "logger/Logger.hpp"

using namespace highp;

class Client {
public:
    explicit Client(std::shared_ptr<log::Logger> logger);

    ~Client() noexcept;

    [[nodiscard]] bool Connect(const char* ipAddress, unsigned short port);

    void Disconnect();

    void Send(const flatbuffers::FlatBufferBuilder& builder);

private:
    std::shared_ptr<log::Logger> _logger;

    std::shared_ptr<net::WsaSession> _wsaSession;

    std::unique_ptr<net::TcpClientSocket> _tcpClientSocket;

    std::unique_ptr<net::PacketStream> _packetStream;

    bool _isConnected = false;
};
