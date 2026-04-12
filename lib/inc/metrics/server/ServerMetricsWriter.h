#pragma once

#include "metrics/server/IServerMetrics.h"
#include "metrics/RunManifest.h"
#include "metrics/server/ServerMetricsConfig.h"
#include "metrics/server/writer/RuntimeSampler.h"
#include "metrics/server/writer/SnapshotRecord.h"
#include "metrics/server/writer/SummaryAccumulator.h"

#include <atomic>
#include <fstream>
#include <memory>
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
        void ThreadMain(const std::stop_token& st);
        bool InitializeArtifacts();
        bool WriteManifest();
        bool CaptureAndWriteSnapshot();
        std::string BuildSnapshotJson(
            const SnapshotRecord* record,
            const SnapshotRecord* previous) const;
        static double ToPerSecond(
            uint64_t delta,
            std::chrono::nanoseconds elapsed) noexcept;
        static double ComputeRate(
            uint64_t current,
            uint64_t prior,
            std::chrono::nanoseconds elapsed) noexcept;

        std::shared_ptr<IServerMetrics> _metrics;
        ServerMetricsConfig _config;
        RunManifest _manifest;

        RuntimeSampler _runtimeSampler;
        SummaryAccumulator _summary;
        SnapshotRecord _previousRecord;

        std::filesystem::path _outputDirectory;
        std::filesystem::path _manifestPath;
        std::filesystem::path _snapshotPath;
        std::filesystem::path _summaryPath;

        std::jthread _workerThread;
        std::atomic<bool> _running{false};
        std::string _lastError;
    };
} // namespace highp::metrics
