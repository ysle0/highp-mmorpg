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

struct ConnectPhase       { int target_count; int ramp_delay_ms; };
struct HoldPhase          { int duration_sec; };
struct SteadySendPhase    { int duration_sec; int send_interval_ms; int message_size_bytes; };

struct PhaseConfig {
    PhaseType type;
    union {
        ConnectPhase       connect;
        HoldPhase          hold;
        SteadySendPhase    steady_send;
    };

    PhaseConfig() : type(PhaseType::Hold), hold{} {}
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
