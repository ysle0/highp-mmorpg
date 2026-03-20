#include <logger/Logger.hpp>
#include <logger/TextLogger.h>
#include "Client.h"
#include "PacketReceiver.h"
#include "ChatCli.h"
#include "client/windows/Client.h"
#include "scope/Defer.h"

int main() {
    auto logger = log::Logger::Default<log::TextLogger>();
    logger->Info("Chat Client starting...");

    Client client(logger);

    // TODO: get ip address from somewhere.
    const std::string ipAddress = "127.0.0.1";
    constexpr uint16_t port = 8080;

    if (!client.Connect(ipAddress, port)) {
        logger->Error("Failed to connect to server.");
        return -1;
    }
    scope::Defer defer([&client] {
        client.Disconnect();
    });

    client.StartRecvLoop([logger](const protocol::Packet* packet) {
        OnPacketReceived(logger, packet);
    });

    ChatCli cli(&client, logger);
    cli.PromptNickname();
    cli.Run();

    return 0;
}
