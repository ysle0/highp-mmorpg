#include "Session.h"

#include "logger/Logger.hpp"

Session::Session(
    std::shared_ptr<highp::log::Logger> logger,
    highp::net::Client* tcpClient
) : _logger(std::move(logger)),
    _tcpClient(tcpClient) {
}

void Session::Send(const flatbuffers::FlatBufferBuilder& builder) {
    if (!_tcpClient || !_tcpClient->IsConnected()) {
        _logger->Error("Not connected to client.");
        return;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const size_t size = builder.GetSize();
    std::string_view msgSpan{reinterpret_cast<const char*>(buf), size};
    if (!_tcpClient->PostSend(msgSpan)) {
        _logger->Error("Send failed.");
        return;
    }

    _logger->Info("Sent: {}", msgSpan.data());
}
