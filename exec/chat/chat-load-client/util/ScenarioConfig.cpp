#include "util/ScenarioConfig.h"

#include <config/runTime/Config.hpp>
#include <format>
#include <stdexcept>

namespace {
    PhaseType parsePhaseType(const std::string& str) {
        if (str == "connect")        return PhaseType::Connect;
        if (str == "join")           return PhaseType::Join;
        if (str == "hold")           return PhaseType::Hold;
        if (str == "steady_send")    return PhaseType::SteadySend;
        if (str == "disconnect")     return PhaseType::Disconnect;
        if (str == "burst_send")     return PhaseType::BurstSend;
        if (str == "reconnect_wave") return PhaseType::ReconnectWave;
        if (str == "malformed_send") return PhaseType::MalformedSend;
        if (str == "role_send")      return PhaseType::RoleSend;
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

        switch (phase.type) {
        case PhaseType::Connect:
            phase.connect = {
                config.Int(std::format("{}.target_count", section), 0),
                config.Int(std::format("{}.ramp_delay_ms", section), 100),
            };
            break;
        case PhaseType::Hold:
            phase.hold = {
                config.Int(std::format("{}.duration_sec", section), 0),
            };
            break;
        case PhaseType::SteadySend:
            phase.steady_send = {
                config.Int(std::format("{}.duration_sec", section), 0),
                config.Int(std::format("{}.send_interval_ms", section), 1000),
                config.Int(std::format("{}.message_size_bytes", section), 64),
            };
            break;
        case PhaseType::BurstSend:
            phase.burst_send = {
                config.Int(std::format("{}.burst_count", section), 5),
                config.Int(std::format("{}.burst_messages", section), 10),
                config.Int(std::format("{}.idle_window_ms", section), 2000),
                config.Int(std::format("{}.message_size_bytes", section), 64),
            };
            break;
        case PhaseType::ReconnectWave:
            phase.reconnect_wave = {
                config.Int(std::format("{}.wave_count", section), 10),
                config.Int(std::format("{}.hold_between_wave_sec", section), 5),
            };
            break;
        case PhaseType::MalformedSend:
            phase.malformed_send = {
                config.Int(std::format("{}.duration_sec", section), 0),
                config.Int(std::format("{}.send_interval_ms", section), 1000),
                config.Int(std::format("{}.message_size_bytes", section), 64),
                config.Int(std::format("{}.malformed_percent", section), 30),
            };
            break;
        case PhaseType::RoleSend:
            phase.role_send = {
                config.Int(std::format("{}.duration_sec", section), 0),
                config.Int(std::format("{}.send_interval_ms", section), 1000),
                config.Int(std::format("{}.message_size_bytes", section), 64),
                config.Int(std::format("{}.sender_count", section), 10),
            };
            break;
        default:
            break;
        }

        sc.phases.push_back(phase);
    }

    if (sc.phases.empty()) {
        throw std::runtime_error("Scenario has no phases defined.");
    }

    return sc;
}
