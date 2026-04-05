#include "Session.h"

#include <cstring>
#include <limits>
#include <vector>

#include "config/Const.h"
#include "logger/Logger.hpp"

Session::Session(
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::net::Client> tcpClient,
    uint64_t sessionId
) : _logger(std::move(logger)),
    _tcpClient(std::move(tcpClient)),
    _sessionId(sessionId) {
}

void Session::Send(const flatbuffers::FlatBufferBuilder& builder) const {
    if (!_tcpClient || !_tcpClient->IsConnected()) {
        _logger->Error("Not connected to client.");
        return;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const size_t size = builder.GetSize();
    if (size > std::numeric_limits<uint32_t>::max()) {
        _logger->Error("Send failed. payload too large: {}", size);
        return;
    }

    const size_t frameSize = sizeof(uint32_t) + size;
    if (frameSize > highp::net::Const::Buffer::sendBufferSize) {
        _logger->Error("Send failed. framed payload exceeds send buffer: {}", frameSize);
        return;
    }

    std::vector<char> frame(frameSize, 0);
    uint32_t payloadSize = htonl(static_cast<uint32_t>(size));
    std::memcpy(frame.data(), &payloadSize, sizeof(payloadSize));
    std::memcpy(frame.data() + sizeof(payloadSize), buf, size);

    const std::string_view msg{frame.data(), frame.size()};
    if (!_tcpClient->PostSend(msg)) {
        _logger->Error("Send failed.");
        return;
    }

    _logger->Info("Sent. sent length: {}", msg.size());
}

bool Session::IsSameSession(const std::shared_ptr<highp::net::Client>& client) const {
    return _tcpClient->socket == client->socket;
}
