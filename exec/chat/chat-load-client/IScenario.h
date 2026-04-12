#pragma once

#include "ScenarioConfig.h"
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
    void ConnectClients(
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
        int durationSec,
        int sendIntervalMs,
        int messageSizeBytes,
        const std::shared_ptr<highp::log::Logger>& logger);

    static void DisconnectAll(
        std::vector<std::unique_ptr<ClientSession>>& sessions,
        const std::shared_ptr<highp::log::Logger>& logger);
};
