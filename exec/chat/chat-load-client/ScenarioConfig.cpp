#include "ScenarioConfig.h"

#include <config/runTime/Config.hpp>
#include <format>
#include <stdexcept>

namespace {
    PhaseType parsePhaseType(const std::string& str) {
        if (str == "connect") return PhaseType::Connect;
        if (str == "join") return PhaseType::Join;
        if (str == "hold") return PhaseType::Hold;
        if (str == "steady_send") return PhaseType::SteadySend;
        if (str == "disconnect") return PhaseType::Disconnect;
        throw std::runtime_error(std::format("Unknown phase type: {}", str));
    }
} // namespace

ScenarioConfig ScenarioConfig::FromFile(const std::string& path) {
    auto optConfig = highp::config::Config::FromFile(path);
    if (!optConfig.has_value()) {
        throw std::runtime_error(std::format("Failed to open scenario file: {}", path));
    }

    auto& config = optConfig.value();

    ScenarioConfig sc;
    sc.name = config.Str("scenario.name", "unnamed");
    sc.description = config.Str("scenario.description", "");
    sc.target_host = config.Str("scenario.target_host", "127.0.0.1");
    sc.target_port = config.Int("scenario.target_port", 8080);

    sc.client_count = config.Int("clients.count", 1);
    sc.nickname_prefix = config.Str("clients.nickname_prefix", "load");

    for (int i = 0; ; ++i) {
        std::string section = std::format("phase_{}", i);
        std::string typeKey = std::format("{}.type", section);

        std::string typeStr = config.Str(typeKey);
        if (typeStr.empty()) {
            break;
        }

        PhaseConfig phase;
        phase.type = parsePhaseType(typeStr);
        phase.duration_sec = config.Int(std::format("{}.duration_sec", section), 0);
        phase.ramp_delay_ms = config.Int(std::format("{}.ramp_delay_ms", section), 100);
        phase.send_interval_ms = config.Int(std::format("{}.send_interval_ms", section), 1000);
        phase.message_size_bytes = config.Int(std::format("{}.message_size_bytes", section), 64);
        phase.target_count = config.Int(std::format("{}.target_count", section), 0);

        sc.phases.push_back(phase);
    }

    if (sc.phases.empty()) {
        throw std::runtime_error("Scenario has no phases defined.");
    }

    return sc;
}
