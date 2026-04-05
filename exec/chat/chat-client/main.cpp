#include <memory>

#include <logger/Logger.hpp>
#include "Client.h"
#include "ChatCli.h"
#include "PacketReceiver.h"
#include "PromptAwareTextLogger.h"
#include "client/windows/Client.h"
#include "scope/Defer.h"

int main() {
    const std::shared_ptr<ConsoleState> console = std::make_shared<ConsoleState>();
    auto logger = std::make_shared<highp::log::Logger>(
        std::make_unique<PromptAwareTextLogger>(console));
    logger->Info("Chat Client starting...");

    Client client(logger);

    // TODO: get ip address from somewhere.
    const std::string ipAddress = "127.0.0.1";
    constexpr uint16_t port = 8080;

    if (!client.Connect(ipAddress, port)) {
        logger->Error("Failed to connect to server.");
        return -1;
    }
    DEFER([&client] {
        client.Disconnect();
        });

    ChatCli cli(&client, logger, console);

    client.StartRecvLoop([logger](const highp::protocol::Packet* packet) {
        onPacketReceived(logger, packet);
    });

    cli.PromptNickname();
    cli.Run();

    return 0;
}
