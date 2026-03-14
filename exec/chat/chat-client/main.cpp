#include <logger/Logger.hpp>
#include <logger/TextLogger.h>
#include "Client.h"
#include "PacketReceiver.h"
#include "ChatCli.h"

using namespace highp;

int main() {
    auto logger = log::Logger::Default<log::TextLogger>();
    logger->Info("Chat Client starting...");

    Client client(logger);
    if (!client.Connect("127.0.0.1", 8080)) {
        logger->Error("Failed to connect to server.");
        return -1;
    }

    client.StartRecvLoop([logger](const protocol::Packet* packet) {
        OnPacketReceived(logger, packet);
    });


    client.Disconnect();
    return 0;
}
