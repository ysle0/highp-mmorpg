#pragma once

#include "Client.h"
#include "logger/ILogger.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

class ClientSession {
public:
    ClientSession(
        int index,
        std::string nickname,
        std::shared_ptr<highp::log::ILogger> innerLogger,
        std::shared_ptr<highp::metrics::IClientMetrics> metrics);

    [[nodiscard]] bool Connect(std::string_view host, unsigned short port);
    void JoinRoom();
    void SendMessage(std::string_view message);
    void SendRawBytes(std::span<const uint8_t> data);
    void Disconnect();
    void StartRecv();

    [[nodiscard]] bool IsConnected() const;
    [[nodiscard]] int Index() const noexcept { return _index; }
    [[nodiscard]] const std::string& Nickname() const noexcept { return _nickname; }

private:
    int _index;
    std::string _nickname;
    Client _client;
};
