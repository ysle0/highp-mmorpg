#include "ClientSession.h"
#include "PacketSender.h"
#include "PrefixedLogger.h"

#include <format>

ClientSession::ClientSession(
    int index,
    std::string nickname,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _index(index),
      _nickname(std::move(nickname)),
      _client(
          std::make_shared<highp::log::Logger>(
              std::make_unique<PrefixedLogger>(
                  std::format("[{}] ", _nickname), std::move(innerLogger))),
          std::move(metrics),
          std::chrono::milliseconds(5000)) {
}

bool ClientSession::Connect(std::string_view host, unsigned short port) {
    return _client.Connect(host, port);
}

void ClientSession::JoinRoom() {
    sendJoinRoom(&_client, _nickname);
}

void ClientSession::SendMessage(std::string_view message) {
    sendMessage(&_client, _nickname, message);
}

void ClientSession::Disconnect() {
    _client.Disconnect();
}

void ClientSession::StartRecv() {
    _client.StartRecvLoop([](const highp::protocol::Packet*) {
        // recv counter is handled by IClientMetrics inside Client
    });
}

bool ClientSession::IsConnected() const {
    return _client.IsConnected();
}
