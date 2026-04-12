// chat-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Server.h"
#include "MetricsCallbacksFactory.h"

#include <config/NetworkCfg.h>
#include <logger/Logger.hpp>
#include <logger/TextLogger.h>
#include <metrics/server/impl/AtomicServerMetrics.h>
#include <metrics/server/impl/NullServerMetrics.h>
#include <metrics/RunManifest.h>
#include <metrics/server/ServerMetricsConfig.h>
#include <metrics/server/ServerMetricsWriter.h>
#include <socket/SocketHelper.h>
#include <socket/SocketOptionBuilder.h>

#include <cstdint>
#include <iostream>
#include <memory>

#include "SessionManager.h"
#include "UserManager.h"
#include "scope/Defer.h"

using namespace highp;

int main() {
    std::shared_ptr<log::Logger> logger = log::Logger::Default<log::TextLogger>();
    net::NetworkCfg c = net::NetworkCfg::FromFile("config.runtime.toml");
    const auto tp = net::NetworkTransport(net::ETransport::TCP);
    const auto sockOptBuilder = std::make_shared<net::SocketOptionBuilder>(logger);

    metrics::ServerMetricsConfig metricsConfig = metrics::ServerMetricsConfig::FromEnvironment();
    std::shared_ptr<metrics::IServerMetrics> metrics;
    if (metricsConfig.enabled) {
        metrics = std::make_shared<metrics::AtomicServerMetrics>();
    } else {
        metrics = std::make_shared<metrics::NullServerMetrics>();
    }

    metrics::RunManifest metricsManifest{
        .serverName = "chat-server",
#if defined(_DEBUG)
        .buildType = "Debug",
#else
        .buildType = "Release",
#endif
        .targetPort = static_cast<uint16_t>(c.server.port),
    };

    auto metricsWriter = std::make_shared<metrics::ServerMetricsWriter>(
        metrics,
        metricsConfig,
        metricsManifest);
    auto metricsCallbacks = MakeMetricsCallbacksBundle(metrics, metricsWriter);

    auto packetDispatcher = std::make_unique<net::PacketDispatcher>(logger);
    auto roomMgr = std::make_shared<RoomManager>(logger, 1, c.room.maxCapacity);
    auto sessionMgr = std::make_shared<SessionManager>(logger);
    auto userMgr = std::make_shared<UserManager>(logger, sessionMgr);
    auto gameLoop = std::make_unique<GameLoop>(
        logger,
        std::move(packetDispatcher),
        std::move(roomMgr),
        std::move(sessionMgr),
        std::move(userMgr),
        c);
    gameLoop->UseCallbacks(std::move(metricsCallbacks.gameLoop));

    Server s(
        logger,
        std::move(gameLoop),
        c,
        sockOptBuilder);
    s.UseCallbacks(std::move(metricsCallbacks.server));
    DEFER([&s] {
        s.Stop();
        });

    const std::shared_ptr<net::ISocket> listenSocket = net::SocketHelper::MakeDefaultListener(
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
