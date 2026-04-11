#include "pch.h"

#include "metrics/server/writer/SummaryAccumulator.h"
#include "src/metrics/server/ServerMetricsSupport.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <sstream>

namespace highp::metrics { namespace {
        double ToPerSecond(uint64_t delta, std::chrono::nanoseconds elapsed) noexcept {
            const double seconds = std::chrono::duration<double>(elapsed).count();
            if (seconds <= 0.0) {
                return 0.0;
            }
            return static_cast<double>(delta) / seconds;
        }
    } // anonymous namespace

    void SummaryAccumulator::Observe(
        const SnapshotRecord* record,
        const SnapshotRecord* previous) {
        ++_snapshots;
        _maxConnectedClients = std::max(_maxConnectedClients, record->connectedClients);
        _maxDispatcherQueueLength = std::max(_maxDispatcherQueueLength, record->dispatcherQueueLength);
        _maxPendingSendCount = std::max(_maxPendingSendCount, record->pendingSendCount);
        _maxQueueWaitMs = std::max(_maxQueueWaitMs, record->queueWaitMsMax);
        _maxDispatchProcessMs = std::max(_maxDispatchProcessMs, record->dispatchProcessMsMax);
        _maxTickDurationMs = std::max(_maxTickDurationMs, record->tickDurationMsMax);
        _maxTickLagMs = std::max(_maxTickLagMs, record->tickLagMsMax);
        _maxProcessCpuPercent = std::max(_maxProcessCpuPercent, record->processCpuPercent);
        _maxLogicThreadCpuPercent = std::max(_maxLogicThreadCpuPercent, record->logicThreadCpuPercent);
        _maxThreadCount = std::max(_maxThreadCount, record->threadCount);
        _disconnectTotal = record->disconnectTotal;
        _packetValidationFailureTotal = record->packetValidationFailureTotal;

        const auto elapsed = previous == nullptr
                                 ? std::chrono::seconds{1}
                                 : std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     record->capturedAt - previous->capturedAt);

        const double acceptPerSec = previous == nullptr ? 0.0 : ComputeRate(record->acceptTotal, previous->acceptTotal, elapsed);
        const double recvPacketsPerSec = previous == nullptr
                                             ? 0.0
                                             : ComputeRate(record->recvPacketsTotal, previous->recvPacketsTotal, elapsed);
        const double sendPacketsPerSec = previous == nullptr
                                             ? 0.0
                                             : ComputeRate(record->sendPacketsTotal, previous->sendPacketsTotal, elapsed);

        _acceptPerSecSum += acceptPerSec;
        _recvPacketsPerSecSum += recvPacketsPerSec;
        _sendPacketsPerSecSum += sendPacketsPerSec;
        _maxAcceptPerSec = std::max(_maxAcceptPerSec, acceptPerSec);
        _maxRecvPacketsPerSec = std::max(_maxRecvPacketsPerSec, recvPacketsPerSec);
        _maxSendPacketsPerSec = std::max(_maxSendPacketsPerSec, sendPacketsPerSec);

        _queueWaitMsSum += record->queueWaitMsAvg;
        _dispatchProcessMsSum += record->dispatchProcessMsAvg;
        _tickDurationMsSum += record->tickDurationMsAvg;
        _tickLagMsSum += record->tickLagMsAvg;
        _processCpuPercentSum += record->processCpuPercent;
        _logicThreadCpuPercentSum += record->logicThreadCpuPercent;
    }

    std::string SummaryAccumulator::BuildMarkdown(
        const RunManifest& manifest,
        const std::filesystem::path& outputDir) const {
        std::ostringstream oss;
        oss << "Server Metrics Summary\n\n";
        oss << "- run_id: " << (manifest.runId.empty() ? "run" : manifest.runId) << "\n";
        oss << "- server_name: " << (manifest.serverName.empty() ? "server" : manifest.serverName) << "\n";
        oss << "- output_dir: " << outputDir.string() << "\n";
        oss << "- started_at: " << (manifest.startedAt ? internal::FormatUtcTime(*manifest.startedAt) : std::string{})
            << "\n";
        oss << "- finished_at: " << (manifest.finishedAt
                                         ? internal::FormatUtcTime(*manifest.finishedAt)
                                         : std::string{}) << "\n\n";

        oss << "Core Results\n";
        oss << "- max connected clients: " << _maxConnectedClients << "\n";
        oss << "- avg accept/sec: " << internal::FormatNumber(Average(_acceptPerSecSum)) << "\n";
        oss << "- max accept/sec: " << internal::FormatNumber(_maxAcceptPerSec) << "\n";
        oss << "- avg recv packets/sec: " << internal::FormatNumber(Average(_recvPacketsPerSecSum)) << "\n";
        oss << "- max recv packets/sec: " << internal::FormatNumber(_maxRecvPacketsPerSec) << "\n";
        oss << "- avg send packets/sec: " << internal::FormatNumber(Average(_sendPacketsPerSecSum)) << "\n";
        oss << "- max send packets/sec: " << internal::FormatNumber(_maxSendPacketsPerSec) << "\n";
        oss << "- avg queue wait ms: " << internal::FormatNumber(Average(_queueWaitMsSum)) << "\n";
        oss << "- max queue wait ms: " << internal::FormatNumber(_maxQueueWaitMs) << "\n";
        oss << "- avg dispatch process ms: " << internal::FormatNumber(Average(_dispatchProcessMsSum)) << "\n";
        oss << "- max dispatch process ms: " << internal::FormatNumber(_maxDispatchProcessMs) << "\n";
        oss << "- avg tick duration ms: " << internal::FormatNumber(Average(_tickDurationMsSum)) << "\n";
        oss << "- max tick duration ms: " << internal::FormatNumber(_maxTickDurationMs) << "\n";
        oss << "- avg tick lag ms: " << internal::FormatNumber(Average(_tickLagMsSum)) << "\n";
        oss << "- max tick lag ms: " << internal::FormatNumber(_maxTickLagMs) << "\n";
        oss << "- max pending send count: " << _maxPendingSendCount << "\n";
        oss << "- packet validation failure total: " << _packetValidationFailureTotal << "\n";
        oss << "- disconnect total: " << _disconnectTotal << "\n";
        oss << "- avg process CPU percent: " << internal::FormatNumber(Average(_processCpuPercentSum)) << "\n";
        oss << "- max process CPU percent: " << internal::FormatNumber(_maxProcessCpuPercent) << "\n";
        oss << "- avg logic thread CPU percent: " << internal::FormatNumber(Average(_logicThreadCpuPercentSum)) <<
            "\n";
        oss << "- max logic thread CPU percent: " << internal::FormatNumber(_maxLogicThreadCpuPercent) << "\n";
        oss << "- max thread count: " << _maxThreadCount << "\n";

        std::string bottleneck = "none observed";
        if (_maxDispatcherQueueLength > 0 && _maxTickLagMs > 0.0) {
            bottleneck = "Queue / Logic";
        }
        else if (_maxPendingSendCount > 0) {
            bottleneck = "Transport / Send";
        }
        else if (_maxProcessCpuPercent >= 80.0) {
            bottleneck = "Runtime / CPU";
        }

        oss << "- bottleneck guess: " << bottleneck << "\n";
        return oss.str();
    }

    void SummaryAccumulator::Reset() noexcept {
        _snapshots = 0;
        _acceptPerSecSum = 0.0;
        _recvPacketsPerSecSum = 0.0;
        _sendPacketsPerSecSum = 0.0;
        _maxAcceptPerSec = 0.0;
        _maxRecvPacketsPerSec = 0.0;
        _maxSendPacketsPerSec = 0.0;
        _queueWaitMsSum = 0.0;
        _dispatchProcessMsSum = 0.0;
        _tickDurationMsSum = 0.0;
        _tickLagMsSum = 0.0;
        _processCpuPercentSum = 0.0;
        _logicThreadCpuPercentSum = 0.0;
        _maxConnectedClients = 0;
        _maxDispatcherQueueLength = 0;
        _maxPendingSendCount = 0;
        _maxQueueWaitMs = 0.0;
        _maxDispatchProcessMs = 0.0;
        _maxTickDurationMs = 0.0;
        _maxTickLagMs = 0.0;
        _maxProcessCpuPercent = 0.0;
        _maxLogicThreadCpuPercent = 0.0;
        _maxThreadCount = 0;
        _disconnectTotal = 0;
        _packetValidationFailureTotal = 0;
    }

    double SummaryAccumulator::ComputeRate(
        uint64_t current,
        uint64_t prior,
        std::chrono::nanoseconds elapsed) noexcept {
        return ToPerSecond(current >= prior ? current - prior : 0, elapsed);
    }

    double SummaryAccumulator::Average(double sum) const noexcept {
        const double count = _snapshots == 0 ? 1.0 : static_cast<double>(_snapshots);
        return sum / count;
    }
} // namespace highp::metrics
