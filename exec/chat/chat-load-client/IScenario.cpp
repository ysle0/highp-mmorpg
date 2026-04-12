#include "IScenario.h"

#include <chrono>
#include <format>
#include <string>
#include <thread>

void IScenario::ConnectClients(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    const ScenarioConfig& config,
    int targetCount,
    int rampDelayMs,
    const std::shared_ptr<highp::log::Logger>& logger,
    const std::shared_ptr<highp::log::ILogger>& innerLogger,
    const std::shared_ptr<highp::metrics::IClientMetrics>& metrics) {

    const int currentCount = static_cast<int>(sessions.size());
    const int toConnect = targetCount - currentCount;

    if (toConnect <= 0) {
        logger->Info("Already have {} sessions, target is {}. Skipping.", currentCount, targetCount);
        return;
    }

    int connected = 0;
    for (int i = 0; i < toConnect; ++i) {
        int index = currentCount + i;
        std::string nickname = std::format("{}-{}", config.nickname_prefix, index);

        auto session = std::make_unique<ClientSession>(index, nickname, innerLogger, metrics);

        if (session->Connect(config.target_host, static_cast<unsigned short>(config.target_port))) {
            session->StartRecv();
            sessions.push_back(std::move(session));
            ++connected;
        }
        else {
            logger->Error("Failed to connect session {}", index);
        }

        if (i + 1 < toConnect && rampDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(rampDelayMs));
        }
    }

    logger->Info("Connected {}/{} new sessions (total: {})", connected, toConnect, sessions.size());
}

void IScenario::JoinAll(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    const std::shared_ptr<highp::log::Logger>& logger) {

    for (auto& session : sessions) {
        session->JoinRoom();
    }
    logger->Info("All {} sessions joined room.", sessions.size());
}

void IScenario::Hold(
    int durationSec,
    const std::shared_ptr<highp::log::Logger>& logger) {

    for (int elapsed = 0; elapsed < durationSec; ++elapsed) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if ((elapsed + 1) % 30 == 0 || elapsed + 1 == durationSec) {
            logger->Info("  hold: {}/{}s", elapsed + 1, durationSec);
        }
    }
}

void IScenario::SteadySend(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    int durationSec,
    int sendIntervalMs,
    int messageSizeBytes,
    const std::shared_ptr<highp::log::Logger>& logger) {

    const auto startTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(durationSec);
    const auto interval = std::chrono::milliseconds(sendIntervalMs);

    std::string message(messageSizeBytes > 0 ? static_cast<size_t>(messageSizeBytes) : 1, 'x');

    auto nextSendTime = startTime + interval;
    int totalSent = 0;
    long long lastLogAt = -1;

    while (std::chrono::steady_clock::now() - startTime < duration) {
        auto now = std::chrono::steady_clock::now();
        if (now >= nextSendTime) {
            for (auto& session : sessions) {
                session->SendMessage(message);
            }
            totalSent += static_cast<int>(sessions.size());
            nextSendTime += interval;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > 0 && elapsed % 30 == 0 && elapsed != lastLogAt) {
            lastLogAt = elapsed;
            logger->Info("  steady_send: {}/{}s, total messages sent: {}",
                         elapsed, durationSec, totalSent);
        }
    }

    logger->Info("Steady send complete. Total messages sent: {}", totalSent);
}

void IScenario::DisconnectAll(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    const std::shared_ptr<highp::log::Logger>& logger) {

    for (auto& session : sessions) {
        session->Disconnect();
    }
    logger->Info("All {} sessions disconnected.", sessions.size());
    sessions.clear();
}
