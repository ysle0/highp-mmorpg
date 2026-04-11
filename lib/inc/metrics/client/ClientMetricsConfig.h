#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace highp::metrics {
    struct ClientMetricsConfig {
        bool enabled = true;
        std::filesystem::path outputRoot{"artifacts/load-tests"};
        std::string runId;
        std::chrono::milliseconds snapshotInterval{1000};
        std::chrono::milliseconds responseTimeout{3000};

        [[nodiscard]] std::filesystem::path ResolveOutputDirectory() const;
        [[nodiscard]] std::string ResolveRunId() const;

        [[nodiscard]] static ClientMetricsConfig FromEnvironment();
    };
} // namespace highp::metrics
