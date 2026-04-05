#include "Client.h"

#include <client/PacketStream.h>
#include <client/TcpClientSocket.h>
#include <client/WsaSession.h>
#include "scope/Defer.h"

Client::Client(std::shared_ptr<highp::log::Logger> logger)
    : _logger(std::move(logger)) {
}

Client::~Client() noexcept {
    if (_isConnected) {
        Disconnect();
    }
}

bool Client::Connect(std::string_view ipAddress, unsigned short port) {
    if (_isConnected) {
        Disconnect();
    }

    auto sessionRes = highp::net::WsaSession::Create(_logger);
    if (sessionRes.HasErr()) {
        _logger->Error("Failed to initialize WSA session.");
        return false;
    }

    _logger->Info("Connecting to {}:{}", ipAddress, port);

    auto wsaSession = sessionRes.Data();
    auto socket = std::make_unique<highp::net::TcpClientSocket>(_logger, wsaSession);
    if (!socket->Connect(ipAddress, port)) {
        _logger->Error("Connect failed.");
        return false;
    }

    _wsaSession = std::move(wsaSession);
    _tcpClientSocket = std::move(socket);
    _packetStream = std::make_unique<highp::net::PacketStream>(*_tcpClientSocket);

    _isConnected = true;
    _logger->Info("Connected to {}:{}", ipAddress, port);
    return true;
}

void Client::Disconnect() {
    DEFER([this] {
        _isConnected = false;
        });

    if (_recvThread.joinable()) {
        _recvThread.request_stop();
    }

    if (!_tcpClientSocket) return;

    _packetStream.reset();
    (void)_tcpClientSocket->Close();
    _tcpClientSocket.reset();

    if (_recvThread.joinable()) {
        _recvThread.join();
    }

    _wsaSession.reset();

    _logger->Info("Disconnected from server.");
}

void Client::Send(const flatbuffers::FlatBufferBuilder& builder) {
    if (!_tcpClientSocket || !_packetStream || !_tcpClientSocket->IsConnected()) {
        _logger->Error("Not connected to server.");
        return;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const size_t size = builder.GetSize();
    auto msgSpan = std::span{buf, size};
    if (!_packetStream->SendFrame(msgSpan)) {
        _logger->Error("Send failed.");
        return;
    }

    // _logger->Info("Sent");
}

void Client::StartRecvLoop(RecvCallback callback) {
    _recvThread = std::jthread([this, cb = std::move(callback)](const std::stop_token& st) {
        std::vector<uint8_t> recvBuffer;

        while (!st.stop_requested() && _isConnected) {
            recvBuffer.clear();

            highp::fn::Result<size_t, highp::err::ENetworkError> res = _packetStream->RecvFrame(recvBuffer);
            if (!res) {
                if (!st.stop_requested() && _isConnected) {
                    _logger->Error("RecvFrame failed.");
                }
                break;
            }

            if (recvBuffer.empty()) continue;

            flatbuffers::Verifier verifier(recvBuffer.data(), recvBuffer.size());
            if (!highp::protocol::VerifyPacketBuffer(verifier)) {
                _logger->Warn("Received invalid packet.");
                continue;
            }

            const highp::protocol::Packet* pkt = highp::protocol::GetPacket(recvBuffer.data());
            if (!pkt) {
                cb(pkt);
            }
        }
    });
}
