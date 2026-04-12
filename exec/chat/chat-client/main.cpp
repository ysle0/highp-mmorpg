#include <memory>

#include <logger/Logger.hpp>
#include <metrics/client/impl/AtomicClientMetrics.h>
#include <metrics/client/ClientMetricsConfig.h>
#include <metrics/client/ClientMetricsWriter.h>
#include <metrics/client/impl/NoopClientMetrics.h>

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

    const std::string ipAddress = "127.0.0.1";
    constexpr uint16_t port = 8080;

    highp::metrics::ClientMetricsConfig metricsConfig =
        highp::metrics::ClientMetricsConfig::FromEnvironment();
    std::shared_ptr<highp::metrics::IClientMetrics> metrics;
    if (metricsConfig.enabled) {
        metrics = std::make_shared<highp::metrics::AtomicClientMetrics>();
    } else {
        metrics = std::make_shared<highp::metrics::NoopClientMetrics>();
    }

    highp::metrics::RunManifest metricsManifest{
        .serverName = "chat-client",
#if defined(_DEBUG)
        .buildType = "Debug",
#else
        .buildType = "Release",
#endif
        .targetHost = ipAddress,
        .targetPort = port,
    };

    auto metricsWriter = std::make_shared<highp::metrics::ClientMetricsWriter>(
        metrics,
        metricsConfig,
        metricsManifest);
    if (!metricsWriter->Start()) {
        logger->Error("Failed to start client metrics writer: {}", metricsWriter->LastError());
    }
    DEFER([metricsWriter] {
        if (metricsWriter) {
            metricsWriter->Stop();
        }
    });

    Client client(logger, metrics, metricsConfig.responseTimeout);

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
