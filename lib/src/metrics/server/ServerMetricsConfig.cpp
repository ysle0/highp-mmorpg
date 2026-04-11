#include "pch.h"

#include "metrics/server/ServerMetricsConfig.h"

#include "src/metrics/server/ServerMetricsSupport.h"

#include <sstream>

namespace highp::metrics {
    ServerMetricsConfig ServerMetricsConfig::FromEnvironment() {
        ServerMetricsConfig config{};
        if (auto value = internal::ReadEnv("HIGHP_METRICS_ENABLED")) {
            config.enabled = internal::ParseBool(*value, config.enabled);
        }

        if (auto value = internal::ReadEnvPath("HIGHP_METRICS_OUTPUT_ROOT"); !value.empty()) {
            config.outputRoot = std::move(value);
        }

        if (auto value = internal::ReadEnv("HIGHP_METRICS_RUN_ID")) {
            config.runId = internal::SanitizePathComponent(*value);
        }

        return config;
    }

    std::filesystem::path ServerMetricsConfig::ResolveOutputDirectory() const {
        return outputRoot / ResolveRunId();
    }

    std::string ServerMetricsConfig::ResolveRunId() const {
        if (!runId.empty()) {
            return internal::SanitizePathComponent(runId);
        }

        std::ostringstream oss;
        oss << internal::FormatCompactTimestamp(std::chrono::system_clock::now())
            << "-p"
            << GetCurrentProcessId();
        return oss.str();
    }
} // namespace highp::metrics
