#pragma once

#include "IScenario.h"
#include "util/ScenarioConfig.h"

class BroadcastFanout final : public IScenario {
public:
    BroadcastFanout(
        ScenarioConfig config,
        std::shared_ptr<highp::log::Logger> logger,
        std::shared_ptr<highp::log::ILogger> innerLogger,
        std::shared_ptr<highp::metrics::IClientMetrics> metrics);

    void Run() override;

    [[nodiscard]] const std::string& Name() const override { return _config.name; }
    [[nodiscard]] const std::string& Description() const override { return _config.description; }

private:
    ScenarioConfig _config;
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<highp::log::ILogger> _innerLogger;
    std::shared_ptr<highp::metrics::IClientMetrics> _metrics;
    std::vector<std::unique_ptr<ClientSession>> _sessions;
};
