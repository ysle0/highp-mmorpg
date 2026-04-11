#pragma once

#include "metrics/server/writer/SnapshotRecord.h"
#include "metrics/RunManifest.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace highp::metrics {
    class SummaryAccumulator final {
    public:
        void Observe(const SnapshotRecord* record, const SnapshotRecord* previous);

        [[nodiscard]] std::string BuildMarkdown(
            const RunManifest& manifest,
            const std::filesystem::path& outputDir) const;

        void Reset() noexcept;

    private:
        static double ComputeRate(
            uint64_t current,
            uint64_t prior,
            std::chrono::nanoseconds elapsed) noexcept;
        [[nodiscard]] double Average(double sum) const noexcept;

        uint64_t _snapshots = 0;
        double _acceptPerSecSum = 0.0;
        double _recvPacketsPerSecSum = 0.0;
        double _sendPacketsPerSecSum = 0.0;
        double _maxAcceptPerSec = 0.0;
        double _maxRecvPacketsPerSec = 0.0;
        double _maxSendPacketsPerSec = 0.0;
        double _queueWaitMsSum = 0.0;
        double _dispatchProcessMsSum = 0.0;
        double _tickDurationMsSum = 0.0;
        double _tickLagMsSum = 0.0;
        double _processCpuPercentSum = 0.0;
        double _logicThreadCpuPercentSum = 0.0;
        int64_t _maxConnectedClients = 0;
        int64_t _maxDispatcherQueueLength = 0;
        int64_t _maxPendingSendCount = 0;
        double _maxQueueWaitMs = 0.0;
        double _maxDispatchProcessMs = 0.0;
        double _maxTickDurationMs = 0.0;
        double _maxTickLagMs = 0.0;
        double _maxProcessCpuPercent = 0.0;
        double _maxLogicThreadCpuPercent = 0.0;
        uint32_t _maxThreadCount = 0;
        uint64_t _disconnectTotal = 0;
        uint64_t _packetValidationFailureTotal = 0;
    };
} // namespace highp::metrics
