#include "pch.h"

#include "metrics/RunManifest.h"
#include "metrics/server/ServerMetricsSupport.h"

#include <sstream>

namespace highp::metrics {
    std::filesystem::path RunManifest::OutputDirectory() const {
        const std::filesystem::path resolvedRunId = runId.empty()
                                                        ? std::filesystem::path{"run"}
                                                        : std::filesystem::path{internal::SanitizePathComponent(runId)};
        return outputRoot / resolvedRunId;
    }

    std::string RunManifest::ToJson() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"run_id\": \"" << internal::JsonEscape(runId) << "\",\n";
        oss << "  \"scenario_name\": \"" << internal::JsonEscape(scenarioName) << "\",\n";
        oss << "  \"server_name\": \"" << internal::JsonEscape(serverName) << "\",\n";
        oss << "  \"git_commit\": \"" << internal::JsonEscape(gitCommit) << "\",\n";
        oss << "  \"git_branch\": \"" << internal::JsonEscape(gitBranch) << "\",\n";
        oss << "  \"build_type\": \"" << internal::JsonEscape(buildType) << "\",\n";
        oss << "  \"machine_info\": \"" << internal::JsonEscape(machineInfo) << "\",\n";
        oss << "  \"target_host\": \"" << internal::JsonEscape(targetHost) << "\",\n";
        oss << "  \"output_root\": \"" << internal::JsonEscape(outputRoot.string()) << "\",\n";
        oss << "  \"output_dir\": \"" << internal::JsonEscape(OutputDirectory().string()) << "\",\n";
        oss << "  \"started_at\": \"" << internal::JsonEscape(startedAt ? internal::FormatUtcTime(*startedAt) : std::string{}) << "\",\n";
        oss << "  \"finished_at\": \"" << internal::JsonEscape(finishedAt ? internal::FormatUtcTime(*finishedAt) : std::string{}) << "\",\n";
        oss << "  \"target_port\": " << targetPort << ",\n";
        oss << "  \"planned_clients\": " << plannedClients << ",\n";
        oss << "  \"message_size_bytes\": " << messageSizeBytes << ",\n";
        oss << "  \"send_interval_ms\": " << sendIntervalMs << ",\n";
        oss << "  \"duration_sec\": " << durationSec << "\n";
        oss << "}\n";
        return oss.str();
    }
} // namespace highp::metrics
