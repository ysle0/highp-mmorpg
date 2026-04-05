#pragma once

#include "metrics/IServerMetrics.h"
#include "metrics/RunManifest.h"
#include "metrics/ServerMetricsConfig.h"
#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <Windows.h>

namespace highp::metrics {
    class ServerMetricsWriter final {
    public:
        explicit ServerMetricsWriter(
            std::shared_ptr<IServerMetrics> metrics,
            ServerMetricsConfig config = {},
            RunManifest manifest = {});

        ~ServerMetricsWriter() noexcept;

        ServerMetricsWriter(const ServerMetricsWriter&) = delete;
        ServerMetricsWriter& operator=(const ServerMetricsWriter&) = delete;
        ServerMetricsWriter(ServerMetricsWriter&&) = delete;
        ServerMetricsWriter& operator=(ServerMetricsWriter&&) = delete;

        [[nodiscard]] bool Start();
        void Stop();

        void SetLogicThreadId(DWORD threadId) noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] std::filesystem::path OutputDirectory() const;
        [[nodiscard]] std::string LastError() const;

    private:
        struct RuntimeSample {
            double processCpuPercent = 0.0;
            double logicThreadCpuPercent = 0.0;
            uint32_t threadCount = 0;
        };

        struct SummaryStats {
            uint64_t snapshots = 0;
            double acceptPerSecSum = 0.0;
            double recvPacketsPerSecSum = 0.0;
            double sendPacketsPerSecSum = 0.0;
            double maxAcceptPerSec = 0.0;
            double maxRecvPacketsPerSec = 0.0;
            double maxSendPacketsPerSec = 0.0;
            double queueWaitMsSum = 0.0;
            double dispatchProcessMsSum = 0.0;
            double tickDurationMsSum = 0.0;
            double tickLagMsSum = 0.0;
            double processCpuPercentSum = 0.0;
            double logicThreadCpuPercentSum = 0.0;
            int64_t maxConnectedClients = 0;
            int64_t maxDispatcherQueueLength = 0;
            int64_t maxPendingSendCount = 0;
            double maxQueueWaitMs = 0.0;
            double maxDispatchProcessMs = 0.0;
            double maxTickDurationMs = 0.0;
            double maxTickLagMs = 0.0;
            double maxProcessCpuPercent = 0.0;
            double maxLogicThreadCpuPercent = 0.0;
            uint32_t maxThreadCount = 0;
            uint64_t disconnectTotal = 0;
            uint64_t packetValidationFailureTotal = 0;
        };

        struct SnapshotRecord {
            std::chrono::system_clock::time_point capturedAt{};
            uint64_t acceptTotal = 0;
            uint64_t disconnectTotal = 0;
            uint64_t recvPacketsTotal = 0;
            uint64_t sendPacketsTotal = 0;
            uint64_t recvBytesTotal = 0;
            uint64_t sendBytesTotal = 0;
            uint64_t sendFailTotal = 0;
            uint64_t packetValidationTotal = 0;
            uint64_t packetValidationFailureTotal = 0;
            int64_t connectedClients = 0;
            int64_t pendingAcceptCount = 0;
            int64_t pendingRecvCount = 0;
            int64_t pendingSendCount = 0;
            int64_t dispatcherQueueLength = 0;
            int64_t runnableWorkerThreadCount = 0;
            double queueWaitMsAvg = 0.0;
            double queueWaitMsMax = 0.0;
            double dispatchProcessMsAvg = 0.0;
            double dispatchProcessMsMax = 0.0;
            double tickDurationMsAvg = 0.0;
            double tickDurationMsMax = 0.0;
            double tickLagMsAvg = 0.0;
            double tickLagMsMax = 0.0;
            double processCpuPercent = 0.0;
            double logicThreadCpuPercent = 0.0;
            uint32_t threadCount = 0;
        };

        void ThreadMain(std::stop_token stopToken);
        bool InitializeArtifacts();
        bool WriteManifest();
        bool CaptureAndWriteSnapshot(bool finalSnapshot);
        RuntimeSample SampleRuntime();
        std::string BuildSnapshotJson(
            const SnapshotRecord& record,
            const SnapshotRecord* previous) const;
        void ObserveSummary(const SnapshotRecord& record, const SnapshotRecord* previous);
        std::string BuildSummaryMarkdown() const;
        static double ToPerSecond(
            uint64_t delta,
            std::chrono::nanoseconds elapsed) noexcept;

        std::shared_ptr<IServerMetrics> _metrics;
        ServerMetricsConfig _config;
        RunManifest _manifest;

        std::filesystem::path _outputDirectory;
        std::filesystem::path _manifestPath;
        std::filesystem::path _snapshotPath;
        std::filesystem::path _summaryPath;

        mutable std::mutex _mutex;
        std::jthread _workerThread;
        std::atomic<bool> _running{false};
        std::atomic<DWORD> _logicThreadId{0};
        DWORD _lastLogicThreadIdSampled{0};
        std::string _lastError;

        SnapshotRecord _previousRecord{};
        bool _hasPreviousRecord = false;
        SummaryStats _summary{};

        std::chrono::steady_clock::time_point _lastRuntimeSampleAt{};
        uint64_t _lastProcessKernel100ns = 0;
        uint64_t _lastProcessUser100ns = 0;
        uint64_t _lastLogicThreadKernel100ns = 0;
        uint64_t _lastLogicThreadUser100ns = 0;
        bool _hasRuntimeBaseline = false;
    };
} // namespace highp::metrics
