// chat-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Server.h"
#include <logger/Logger.hpp>
#include <config/NetworkCfg.h>
#include <socket/SocketHelper.h>
#include <socket/SocketOptionBuilder.h>
#include <logger/TextLogger.h>
#include <iostream>

#include "SessionManager.h"
#include "scope/Defer.h"

using namespace highp;

int main() {
    auto logger = log::Logger::Default<log::TextLogger>();
    auto c = net::NetworkCfg::FromFile("config.runtime.toml");
    auto tp = net::NetworkTransport(net::ETransport::TCP);
    auto sockOptBuilder = std::make_shared<net::SocketOptionBuilder>(logger);
    auto packetDispatcher = std::make_unique<net::PacketDispatcher>(logger);
    auto roomMgr = std::make_unique<RoomManager>(logger, 1, c.room.maxCapacity);
    auto sessionMgr = std::make_unique<SessionManager>(logger);
    auto gameLoop = std::make_unique<GameLoop>(
        logger,
        std::move(packetDispatcher),
        std::move(roomMgr),
        std::move(sessionMgr),
        c);

    Server s(logger, std::move(gameLoop), c, sockOptBuilder);
    DEFER([&s] {
        s.Stop();
        });

    auto listenSocket = net::SocketHelper::MakeDefaultListener(
        logger,
        tp,
        c,
        sockOptBuilder);

    if (!s.Start(listenSocket)) {
        logger->Error("Failed to start server.");
        return -1;
    }

    logger->Info("Press Enter to stop the server...");
    std::cin.get();

    return 0;
}
