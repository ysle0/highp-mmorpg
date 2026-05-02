#include "util/ScenarioConfig.h"
#include "util/ResultCollector.h"

#include "IScenario.h"

#include "scenarios/baseline-idle/BaselineIdle.h"
#include "scenarios/step-connect/StepConnect.h"
#include "scenarios/steady-small-traffic/SteadySmallTraffic.h"

#include "logger/Logger.hpp"
#include "logger/TextLogger.h"
#include "metrics/client/impl/AtomicClientMetrics.h"

#include <chrono>
#include <format>
#include <iostream>
#include <memory>

#ifdef _WIN32
#include <Windows.h>
#endif

static std::unique_ptr<IScenario> CreateScenario(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics);

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc < 2) {
        std::cerr << "Usage: chat-load-client <scenario-dir>\n";
        std::cerr << "  e.g. chat-load-client scenarios/baseline-idle\n";
        return 1;
    }

    try {
        std::string scenarioDir = argv[1];
        std::string tomlPath = scenarioDir + "/scenario.toml";

        ScenarioConfig config = ScenarioConfig::FromFile(tomlPath);

        auto innerLogger = std::make_shared<highp::log::TextLogger>();
        auto logger = std::make_shared<highp::log::Logger>(
            std::make_unique<highp::log::TextLogger>());
        auto metrics = std::make_shared<highp::metrics::AtomicClientMetrics>();

        logger->Info("=== Load Test: {} ===", config.name);
        if (!config.description.empty()) {
            logger->Info("{}", config.description);
        }
        logger->Info("Target: {}:{}, Clients: {}, Phases: {}",
                     config.target_host, config.target_port,
                     config.client_count, config.phases.size());

        auto scenario = CreateScenario(config, logger, innerLogger, metrics);
        if (!scenario) {
            std::cerr << "Unknown scenario: " << config.name << "\n";
            return 1;
        }

        const auto startTime = std::chrono::steady_clock::now();

        scenario->Run();

        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();

        std::string resultDir = ResultCollector::WriteJson(
            config.name,
            metrics,
            static_cast<int>(elapsed));

        logger->Info("Results written to {}", resultDir);
        logger->Info("=== Load Test Complete ({} seconds) ===", elapsed);
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

static std::unique_ptr<IScenario> CreateScenario(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics) {
    const std::string& name = config.name;

    if (name == "baseline-idle") {
        return std::make_unique<BaselineIdle>(
            std::move(config),
            std::move(logger),
            std::move(innerLogger),
            std::move(metrics));
    }
    if (name == "step-connect") {
        return std::make_unique<StepConnect>(
            std::move(config),
            std::move(logger),
            std::move(innerLogger),
            std::move(metrics));
    }
    if (name == "steady-small-traffic" || name == "soak-30m") {
        return std::make_unique<SteadySmallTraffic>(
            std::move(config),
            std::move(logger),
            std::move(innerLogger),
            std::move(metrics));
    }
    }
    if (name == "steady-small-traffic") {
        return std::make_unique<SteadySmallTraffic>(std::move(config), std::move(logger), std::move(innerLogger), std::move(metrics));
    }

    return nullptr;
}
