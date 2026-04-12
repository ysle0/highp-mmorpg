#pragma once

#include <stdexcept>
#include <string>
#include <vector>

enum class PhaseType {
    Connect,
    Join,
    Hold,
    SteadySend,
    Disconnect,
};

struct PhaseConfig {
    PhaseType type = PhaseType::Hold;
    int duration_sec = 0;
    int ramp_delay_ms = 100;
    int send_interval_ms = 1000;
    int message_size_bytes = 64;
    int target_count = 0;       // 0 = use clients.count
};

struct ScenarioConfig {
    std::string name;
    std::string description;
    std::string target_host = "127.0.0.1";
    int target_port = 8080;

    int client_count = 1;
    std::string nickname_prefix = "load";

    std::vector<PhaseConfig> phases;

    [[nodiscard]] const PhaseConfig& FindPhase(PhaseType type) const {
        for (const auto& phase : phases) {
            if (phase.type == type) return phase;
        }
        throw std::runtime_error("Required phase not found in scenario config.");
    }

    [[nodiscard]] static ScenarioConfig FromFile(const std::string& path);
};
