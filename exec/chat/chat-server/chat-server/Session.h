#pragma once
#include <cstdint>

#include <flatbuffers/flatbuffers.h>

#include "client/windows/Client.h"
#include "logger/Logger.hpp"

class Session final {
public:
    explicit Session(
        std::shared_ptr<highp::log::Logger> logger,
        std::shared_ptr<highp::net::Client> tcpClient,
        uint64_t sessionId
    );

    void Send(const flatbuffers::FlatBufferBuilder& builder) const;
    [[nodiscard]] bool IsSameSession(const std::shared_ptr<highp::net::Client>& client) const;
    [[nodiscard]] uint64_t GetId() const { return _sessionId; }

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<highp::net::Client> _tcpClient;
    uint64_t _sessionId{0};
};
