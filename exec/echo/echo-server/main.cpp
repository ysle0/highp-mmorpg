#include "Server.h"
#include <logger/Logger.hpp>
#include <config/NetworkCfg.h>
#include <socket/SocketHelper.h>
#include <socket/SocketOptionBuilder.h>
#include <logger/TextLogger.h>
#include <iostream>

using namespace highp;

int main() {
    auto logger = log::Logger::Default<log::TextLogger>();
    auto config = network::NetworkCfg::FromFile("config.runtime.toml");
    auto transport = network::NetworkTransport(network::ETransport::TCP);
    auto socketOptionBuilder = std::make_shared<network::SocketOptionBuilder>(logger);

    auto listenSocket = network::SocketHelper::MakeDefaultListener(
        logger,
        transport,
        config,
        socketOptionBuilder);

    Server es(logger, config, socketOptionBuilder);
    if (!es.Start(listenSocket)) {
        logger->Error("Failed to start server.");
        return -1;
    }

    logger->Info("Press Enter to stop the server...");
    std::cin.get();

    es.Stop();
    return 0;
}
