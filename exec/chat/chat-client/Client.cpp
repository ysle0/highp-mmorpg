#include "Client.h"

#include <client/PacketStream.h>
#include <client/TcpClientSocket.h>
#include <client/WsaSession.h>

std::atomic_uint32_t Client::_nextSequence{1};

namespace {
    [[nodiscard]] const char* toDisconnectMessage(highp::metrics::ClientDisconnectReason reason) noexcept {
        switch (reason) {
        case highp::metrics::ClientDisconnectReason::Timeout:
            return "Disconnected from server (timeout).";
        case highp::metrics::ClientDisconnectReason::ServerClose:
            return "Disconnected from server (server close).";
        case highp::metrics::ClientDisconnectReason::RecvFail:
            return "Disconnected from server (recv fail).";
        case highp::metrics::ClientDisconnectReason::LocalClose:
            return "Disconnected from server.";
        case highp::metrics::ClientDisconnectReason::None:
        default:
            return "Disconnected from server (unknown).";
        }
    }
} // namespace

Client::Client(
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics,
    std::chrono::milliseconds responseTimeout)
    : _logger(std::move(logger)),
      _metrics(std::move(metrics)),
      _responseTimeout(responseTimeout) {
}

Client::~Client() noexcept {
    if (_isConnected.load() || _recvThread.joinable() || _tcpClientSocket || _wsaSession) {
        Disconnect();
    }
}

bool Client::Connect(std::string_view ipAddress, unsigned short port) {
    if (_isConnected.load()) {
        Disconnect();
    }

    const auto connectStartedAt = std::chrono::steady_clock::now();

    const highp::net::WsaSession::ResWithSession sessionRes = highp::net::WsaSession::Create(_logger);
    if (sessionRes.HasErr()) {
        if (_metrics) {
            _metrics->ObserveConnectLatency(std::chrono::steady_clock::now() - connectStartedAt, false);
        }
        _logger->Error("Failed to initialize WSA session.");
        return false;
    }

    _logger->Info("Connecting to {}:{}", ipAddress, port);

    auto wsaSession = sessionRes.Data();
    auto socket = std::make_unique<highp::net::TcpClientSocket>(_logger, wsaSession);
    if (!socket->Connect(ipAddress, port)) {
        if (_metrics) {
            _metrics->ObserveConnectLatency(std::chrono::steady_clock::now() - connectStartedAt, false);
        }
        _logger->Error("Connect failed.");
        return false;
    }

    _wsaSession = std::move(wsaSession);
    _tcpClientSocket = std::move(socket);
    _packetStream = std::make_unique<highp::net::PacketStream>(*_tcpClientSocket);

    _isConnected.store(true);
    if (_metrics) {
        _metrics->ObserveConnectLatency(std::chrono::steady_clock::now() - connectStartedAt, true);
        _metrics->OnConnected();
    }
    _logger->Info("Connected to {}:{}", ipAddress, port);
    return true;
}

void Client::Disconnect(highp::metrics::ClientDisconnectReason reason) {
    const bool wasConnected = _isConnected.exchange(false);

    if (_recvThread.joinable()) {
        _recvThread.request_stop();
    }

    if (_tcpClientSocket) {
        (void)_tcpClientSocket->Close();
    }

    if (_recvThread.joinable()) {
        _recvThread.join();
    }

    _packetStream.reset();
    _tcpClientSocket.reset();
    _wsaSession.reset();

    if (wasConnected) {
        if (_metrics) {
            _metrics->OnDisconnected(reason);
        }
        _logger->Info(toDisconnectMessage(reason));
    }
}

uint32_t Client::NextSequence() noexcept {
    return _nextSequence.fetch_add(1, std::memory_order_relaxed);
}

void Client::Send(const flatbuffers::FlatBufferBuilder& builder, RequestTracking tracking) {
    if (!_tcpClientSocket || !_packetStream || !_tcpClientSocket->IsConnected()) {
        _logger->Error("Not connected to server.");
        return;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const size_t size = builder.GetSize();
    auto msgSpan = std::span{buf, size};
    if (!_packetStream->SendFrame(msgSpan)) {
        if (_metrics) {
            _metrics->OnSendFailed();
        }
        _logger->Error("Send failed.");
        return;
    }

    if (_metrics) {
        _metrics->OnSendCompleted(size);
        if (tracking.sequence != 0 &&
            tracking.responseType != highp::protocol::MessageType::None) {
            _metrics->TrackRequest(
                tracking.sequence,
                static_cast<int32_t>(tracking.responseType),
                _responseTimeout);
        }
    }
}

void Client::StartRecvLoop(RecvCallback callback) {
    _recvThread = std::jthread([this, cb = std::move(callback)](const std::stop_token& st) {
        std::vector<uint8_t> recvBuffer;

        while (!st.stop_requested() && _isConnected.load()) {
            recvBuffer.clear();

            highp::fn::Result<size_t, highp::err::ENetworkError> res = _packetStream->RecvFrame(recvBuffer);
            if (!res) {
                if (!st.stop_requested() && _isConnected.load()) {
                    const highp::metrics::ClientDisconnectReason disconnectReason =
                        res.Err() == highp::err::ENetworkError::SocketInvalid
                            ? highp::metrics::ClientDisconnectReason::ServerClose
                            : highp::metrics::ClientDisconnectReason::RecvFail;
                    _isConnected.store(false);
                    if (_tcpClientSocket) {
                        (void)_tcpClientSocket->Close();
                    }
                    if (_metrics) {
                        _metrics->OnDisconnected(disconnectReason);
                    }
                    _logger->Error(toDisconnectMessage(disconnectReason));
                }
                break;
            }

            if (recvBuffer.empty()) {
                continue;
            }

            if (_metrics) {
                _metrics->OnRecvCompleted(recvBuffer.size());
            }

            flatbuffers::Verifier verifier(recvBuffer.data(), recvBuffer.size());
            if (!highp::protocol::VerifyPacketBuffer(verifier)) {
                if (_metrics) {
                    _metrics->OnPacketValidationFailed();
                }
                _logger->Warn("Received invalid packet.");
                continue;
            }

            const highp::protocol::Packet* pkt = highp::protocol::GetPacket(recvBuffer.data());
            if (pkt) {
                if (_metrics) {
                    _metrics->OnPacketValidationSucceeded();
                    _metrics->ResolveRequest(pkt->sequence(), static_cast<int32_t>(pkt->type()));
                }
                cb(pkt);
            }
        }
    });
}
