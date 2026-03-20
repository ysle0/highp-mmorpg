#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <flatbuf/gen/packet_generated.h>
#include <flatbuffers/flatbuffer_builder.h>
#include "logger/Logger.hpp"

namespace highp::net {
    class WsaSession;
    class TcpClientSocket;
    class PacketStream;
}


class Client {
public:
    using RecvCallback = std::function<void(const highp::protocol::Packet*)>;

    explicit Client(std::shared_ptr<highp::log::Logger> logger);

    ~Client() noexcept;

    [[nodiscard]] bool Connect(std::string_view ipAddress, unsigned short port);

    void Disconnect();

    void Send(const flatbuffers::FlatBufferBuilder& builder);

    void StartRecvLoop(RecvCallback callback);

private:
    std::shared_ptr<highp::log::Logger> _logger;

    std::shared_ptr<highp::net::WsaSession> _wsaSession;

    std::unique_ptr<highp::net::TcpClientSocket> _tcpClientSocket;

    std::unique_ptr<highp::net::PacketStream> _packetStream;

    std::jthread _recvThread;

    bool _isConnected = false;
};
