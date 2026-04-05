#include "Client.h"

#include "scope/Defer.h"

Client::Client(std::shared_ptr<log::Logger> logger) : _logger(logger) {
}

Client::~Client() noexcept {
    if (_isConnected) {
        Disconnect();
    }
}

bool Client::Connect(const char* ipAddress, unsigned short port) {
    if (_isConnected) {
        Disconnect();
    }

    auto sessionRes = net::WsaSession::Create(_logger);
    if (sessionRes.HasErr()) {
        _logger->Error("Failed to initialize WSA session.");
        return false;
    }

    _logger->Info("Connecting to {}:{}", ipAddress, port);

    auto wsaSession = sessionRes.Data();
    auto socket = std::make_unique<net::TcpClientSocket>(_logger, wsaSession);
    if (!socket->Connect(ipAddress, port)) {
        _logger->Error("Connect failed.");
        return false;
    }

    _wsaSession = std::move(wsaSession);
    _tcpClientSocket = std::move(socket);
    _packetStream = std::make_unique<net::PacketStream>(*_tcpClientSocket);

    _isConnected = true;
    _logger->Info("Connected to {}:{}", ipAddress, port);
    return true;
}

void Client::Disconnect() {
    DEFER([this] {
        _isConnected = false;
    });

    if (!_tcpClientSocket) return;

    _packetStream.reset();
    (void)_tcpClientSocket->Close();
    _tcpClientSocket.reset();
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

    _logger->Info("Sent");
}
