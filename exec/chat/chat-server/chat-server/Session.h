#pragma once
#include <flatbuffers/flatbuffers.h>

#include "client/windows/Client.h"
#include "logger/Logger.hpp"

class Session {
public:
    explicit Session(
        std::shared_ptr<highp::log::Logger> logger,
        highp::net::Client* tcpClient
    );

    void Send(const flatbuffers::FlatBufferBuilder& builder);

private:
    std::shared_ptr<highp::log::Logger> _logger;
    std::unique_ptr<highp::net::Client> _tcpClient;
};
