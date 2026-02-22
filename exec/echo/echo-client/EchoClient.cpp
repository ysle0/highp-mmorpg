#include <logger/Logger.hpp>
#include <span>
#include <utility>
#include <vector>
#include "EchoClient.h"

EchoClient::EchoClient(std::shared_ptr<Logger> logger)
    : _logger(logger) {
    //
}

EchoClient::~EchoClient() noexcept {
    Disconnect();
}

bool EchoClient::Connect(const char* ipAddress, unsigned short port) {
    Disconnect();

    auto sessionRes = net::WsaSession::Create(_logger);
    if (sessionRes.HasErr()) {
        _logger->Error("Failed to initialize WSA session.");
        return false;
    }

    _logger->Info("Connecting to {}:{}", ipAddress, port);

    auto wsaSession = sessionRes.Data();
    auto socket = std::make_unique<net::TcpClientSocket>(_logger, wsaSession);
    if (auto res = socket->Connect(ipAddress, port); res.HasErr()) {
        _logger->Error("Connect failed.");
        return false;
    }

    _wsaSession = std::move(wsaSession);
    _tcpClientSocket = std::move(socket);
    _packetStream = std::make_unique<net::PacketStream>(*_tcpClientSocket);

    _logger->Info("Connected to {}:{}", ipAddress, port);
    return true;
}

bool EchoClient::Disconnect() {
    if (!_tcpClientSocket) {
        return false;
    }

    _packetStream.reset();
    _tcpClientSocket->Close();
    _tcpClientSocket.reset();
    _wsaSession.reset();

    _logger->Info("Disconnected from server.");
    return true;
}

void EchoClient::Send(std::string_view message) {
    if (!_tcpClientSocket || !_packetStream || !_tcpClientSocket->IsConnected()) {
        _logger->Error("Not connected to server.");
        return;
    }

    auto msgSpan = std::span{
        reinterpret_cast<const uint8_t*>(message.data()),
        message.size()
    };
    if (!_packetStream->SendFrame(msgSpan)) {
        _logger->Error("Send failed.");
        return;
    }
    _logger->Info("Sent: {}", message);

    std::vector<uint8_t> recvBuffer;
    if (!_packetStream->RecvFrame(recvBuffer)) {
        _logger->Error("Recv failed.");
        return;
    }

    if (recvBuffer.empty()) {
        _logger->Info("Received: ");
        return;
    }

    std::string_view recvData{
        reinterpret_cast<const char*>(recvBuffer.data()),
        recvBuffer.size()
    };
    _logger->Info("Received: {}", recvData);
}
