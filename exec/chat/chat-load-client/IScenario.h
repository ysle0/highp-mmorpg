#pragma once

#include "util/ScenarioConfig.h"
#include "ClientSession.h"

#include "logger/Logger.hpp"
#include "logger/ILogger.h"
#include "metrics/client/IClientMetrics.h"

#include <memory>
#include <string>
#include <vector>

class IScenario {
public:
    virtual ~IScenario() = default;

    virtual void Run() = 0;

    [[nodiscard]] virtual const std::string& Name() const = 0;
    [[nodiscard]] virtual const std::string& Description() const = 0;

protected:
    static void ConnectClients(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        const ScenarioConfig& config,
        int targetCount,
        int rampDelayMs,
        const std::shared_ptr<highp::log::Logger>& logger,
        const std::shared_ptr<highp::log::ILogger>& innerLogger,
        const std::shared_ptr<highp::metrics::IClientMetrics>& metrics);

    static void JoinAll(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        const std::shared_ptr<highp::log::Logger>& logger);

    static void Hold(
        int durationSec,
        const std::shared_ptr<highp::log::Logger>& logger);

    static void SteadySend(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        int sendDurationSec,
        int sendIntervalMs,
        int messageSizeBytes,
        const std::shared_ptr<highp::log::Logger>& logger);

    static void DisconnectAll(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        const std::shared_ptr<highp::log::Logger>& logger);

    static void BurstSend(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        int burstCount,
        int burstMessages,
        int idleWindowMs,
        int messageSizeBytes,
        const std::shared_ptr<highp::log::Logger>& logger);

    static void ReconnectWave(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        const ScenarioConfig& config,
        int waveCount,
        int holdBetweenWaveSec,
        const std::shared_ptr<highp::log::Logger>& logger,
        const std::shared_ptr<highp::log::ILogger>& innerLogger,
        const std::shared_ptr<highp::metrics::IClientMetrics>& metrics);

    static void MalformedSend(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        int durationSec,
        int sendIntervalMs,
        int messageSizeBytes,
        int malformedPercent,
        const std::shared_ptr<highp::log::Logger>& logger);

    static void RoleSend(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        int senderCount,
        int durationSec,
        int sendIntervalMs,
        int messageSizeBytes,
        const std::shared_ptr<highp::log::Logger>& logger);
};
