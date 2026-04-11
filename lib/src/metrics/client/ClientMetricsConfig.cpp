#include "pch.h"

#include "metrics/client/ClientMetricsConfig.h"

#include "src/metrics/server/ServerMetricsSupport.h"

#include <algorithm>
#include <limits>
#include <sstream>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace highp::metrics {
    ClientMetricsConfig ClientMetricsConfig::FromEnvironment() {
        ClientMetricsConfig config{};
        if (auto value = internal::ReadEnv("HIGHP_METRICS_ENABLED")) {
            config.enabled = internal::ParseBool(*value, config.enabled);
        }

        if (auto value = internal::ReadEnvPath("HIGHP_METRICS_OUTPUT_ROOT"); !value.empty()) {
            config.outputRoot = std::move(value);
        }

        if (auto value = internal::ReadEnv("HIGHP_METRICS_RUN_ID")) {
            config.runId = internal::SanitizePathComponent(*value);
        }

        if (auto value = internal::ReadEnv("HIGHP_CLIENT_METRICS_RESPONSE_TIMEOUT_MS")) {
            const uint64_t timeoutMs = internal::ParseUnsigned(
                *value,
                static_cast<uint64_t>(config.responseTimeout.count()));
            const auto clamped = (std::min<uint64_t>)(
                timeoutMs,
                static_cast<uint64_t>((std::numeric_limits<int64_t>::max)()));
            config.responseTimeout = std::chrono::milliseconds(clamped);
        }

        return config;
    }

    std::filesystem::path ClientMetricsConfig::ResolveOutputDirectory() const {
        return outputRoot / ResolveRunId();
    }

    std::string ClientMetricsConfig::ResolveRunId() const {
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
