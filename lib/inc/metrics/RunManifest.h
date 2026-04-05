#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace highp::metrics {
    struct RunManifest {
        std::string runId;
        std::string scenarioName;
        std::string serverName;
        std::string gitCommit;
        std::string gitBranch;
        std::string buildType;
        std::string machineInfo;
        std::string targetHost;
        std::filesystem::path outputRoot{"artifacts/load-tests"};

        std::optional<std::chrono::system_clock::time_point> startedAt;
        std::optional<std::chrono::system_clock::time_point> finishedAt;

        uint16_t targetPort = 0;
        int plannedClients = 0;
        int messageSizeBytes = 0;
        int sendIntervalMs = 0;
        int durationSec = 0;

        [[nodiscard]] std::filesystem::path OutputDirectory() const;
        [[nodiscard]] std::string ToJson() const;
    };
} // namespace highp::metrics
