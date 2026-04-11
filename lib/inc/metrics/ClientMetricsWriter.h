#pragma once

#include "metrics/ClientMetricsConfig.h"
#include "metrics/IClientMetrics.h"
#include "metrics/RunManifest.h"

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

namespace highp::metrics {
    class ClientMetricsWriter final {
    public:
        explicit ClientMetricsWriter(
            std::shared_ptr<IClientMetrics> metrics,
            ClientMetricsConfig config = {},
            RunManifest manifest = {});

        ~ClientMetricsWriter() noexcept;

        ClientMetricsWriter(const ClientMetricsWriter&) = delete;
        ClientMetricsWriter& operator=(const ClientMetricsWriter&) = delete;
        ClientMetricsWriter(ClientMetricsWriter&&) = delete;
        ClientMetricsWriter& operator=(ClientMetricsWriter&&) = delete;

        [[nodiscard]] bool Start();
        void Stop();

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] std::filesystem::path OutputDirectory() const;
        [[nodiscard]] std::string LastError() const;

    private:
        struct RuntimeSample {
            double processCpuPercent = 0.0;
            uint32_t threadCount = 0;
        };

        struct SummaryStats {
            uint64_t snapshots = 0;
            double avgConnectLatencyMsSum = 0.0;
            double avgRequestRttMsSum = 0.0;
            double sendPacketsPerSecSum = 0.0;
            double recvPacketsPerSecSum = 0.0;
            double sendBytesPerSecSum = 0.0;
            double recvBytesPerSecSum = 0.0;
            double processCpuPercentSum = 0.0;
            double maxConnectLatencyMs = 0.0;
            double maxRequestRttMs = 0.0;
            double maxSendPacketsPerSec = 0.0;
            double maxRecvPacketsPerSec = 0.0;
            double maxSendBytesPerSec = 0.0;
            double maxRecvBytesPerSec = 0.0;
            double maxProcessCpuPercent = 0.0;
            uint64_t maxSessionUptimeMs = 0;
            uint64_t maxTimeSinceLastRecvMs = 0;
            uint32_t maxPendingRequestCount = 0;
            uint32_t maxThreadCount = 0;
            uint64_t connectSuccessTotal = 0;
            uint64_t connectFailureTotal = 0;
            uint64_t disconnectTotal = 0;
            uint64_t sendFailTotal = 0;
            uint64_t responseTimeoutTotal = 0;
            uint64_t packetValidationFailureTotal = 0;
            uint64_t disconnectTimeoutTotal = 0;
            uint64_t disconnectServerCloseTotal = 0;
            uint64_t disconnectRecvFailTotal = 0;
            uint64_t disconnectLocalCloseTotal = 0;
            ClientDisconnectReason lastDisconnectReason = ClientDisconnectReason::None;
        };

        struct SnapshotRecord {
            std::chrono::system_clock::time_point capturedAt{};
            uint64_t connectSuccessTotal = 0;
            uint64_t connectFailureTotal = 0;
            uint64_t disconnectTotal = 0;
            uint64_t sendPacketsTotal = 0;
            uint64_t recvPacketsTotal = 0;
            uint64_t sendBytesTotal = 0;
            uint64_t recvBytesTotal = 0;
            uint64_t sendFailTotal = 0;
            uint64_t responseTimeoutTotal = 0;
            uint64_t packetValidationTotal = 0;
            uint64_t packetValidationFailureTotal = 0;
            uint64_t disconnectTimeoutTotal = 0;
            uint64_t disconnectServerCloseTotal = 0;
            uint64_t disconnectRecvFailTotal = 0;
            uint64_t disconnectLocalCloseTotal = 0;
            int64_t connected = 0;
            uint32_t pendingRequestCount = 0;
            double connectLatencyMsAvg = 0.0;
            double connectLatencyMsMax = 0.0;
            double requestRttMsAvg = 0.0;
            double requestRttMsMax = 0.0;
            double lastConnectLatencyMs = 0.0;
            uint64_t currentSessionUptimeMs = 0;
            uint64_t lastSessionUptimeMs = 0;
            uint64_t maxSessionUptimeMs = 0;
            uint64_t timeSinceLastRecvMs = 0;
            ClientDisconnectReason lastDisconnectReason = ClientDisconnectReason::None;
            double processCpuPercent = 0.0;
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

        std::shared_ptr<IClientMetrics> _metrics;
        ClientMetricsConfig _config;
        RunManifest _manifest;

        std::filesystem::path _outputDirectory;
        std::filesystem::path _manifestPath;
        std::filesystem::path _snapshotPath;
        std::filesystem::path _summaryPath;

        mutable std::mutex _mutex;
        std::jthread _workerThread;
        std::atomic<bool> _running{false};
        std::string _lastError;

        SnapshotRecord _previousRecord{};
        bool _hasPreviousRecord = false;
        SummaryStats _summary{};

        std::chrono::steady_clock::time_point _lastRuntimeSampleAt{};
        uint64_t _lastProcessKernel100ns = 0;
        uint64_t _lastProcessUser100ns = 0;
        bool _hasRuntimeBaseline = false;
    };
} // namespace highp::metrics
