#include "Session.h"

#include "logger/Logger.hpp"

Session::Session(
    std::shared_ptr<highp::log::Logger> logger,
    uint64_t sessionId,
    std::shared_ptr<highp::net::Client> tcpClient
) : _logger(std::move(logger)),
    _sessionId(sessionId),
    _tcpClient(std::move(tcpClient)) {
}

void Session::Send(const flatbuffers::FlatBufferBuilder& builder) const {
    if (!_tcpClient || !_tcpClient->IsConnected()) {
        _logger->Error("Not connected to client.");
        return;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const size_t size = builder.GetSize();
    const std::string_view msg{reinterpret_cast<const char*>(buf), size};
    if (!_tcpClient->PostSend(msg)) {
        _logger->Error("Send failed.");
        return;
    }

    _logger->Info("Sent. sent length: {}", msg.size());
}

bool Session::IsSameSession(const std::shared_ptr<highp::net::Client>& client) const {
    return _tcpClient->socket == client->socket;
}
