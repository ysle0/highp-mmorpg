#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

#include <flatbuf/gen/packet_generated.h>
#include <flatbuffers/flatbuffer_builder.h>

#include "logger/Logger.hpp"
#include "metrics/client/IClientMetrics.h"

namespace highp::net {
    class WsaSession;
    class TcpClientSocket;
    class PacketStream;
}

class Client {
public:
    using RecvCallback = std::function<void(const highp::protocol::Packet*)>;

    struct RequestTracking {
        uint32_t sequence = 0;
        highp::protocol::MessageType responseType = highp::protocol::MessageType::None;
    };

    explicit Client(
        std::shared_ptr<highp::log::Logger> logger,
        std::shared_ptr<highp::metrics::IClientMetrics> metrics,
        std::chrono::milliseconds responseTimeout);

    ~Client() noexcept;

    [[nodiscard]] bool Connect(std::string_view ipAddress, unsigned short port);

    void Disconnect(
        highp::metrics::ClientDisconnectReason reason =
            highp::metrics::ClientDisconnectReason::LocalClose);

    [[nodiscard]] uint32_t NextSequence() noexcept;

    void Send(
        const flatbuffers::FlatBufferBuilder& builder,
        RequestTracking tracking = {});

    void StartRecvLoop(RecvCallback callback);

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<highp::metrics::IClientMetrics> _metrics;

    std::chrono::milliseconds _responseTimeout;

    std::shared_ptr<highp::net::WsaSession> _wsaSession;

    std::unique_ptr<highp::net::TcpClientSocket> _tcpClientSocket;

    std::unique_ptr<highp::net::PacketStream> _packetStream;

    std::jthread _recvThread;

    std::atomic_bool _isConnected = false;
    std::atomic_uint32_t _nextSequence{1};
};
