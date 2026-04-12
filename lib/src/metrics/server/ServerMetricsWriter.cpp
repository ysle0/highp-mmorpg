#include "pch.h"

#include "metrics/server/ServerMetricsWriter.h"
#include "metrics/server/writer/JsonFields.hpp"
#include "metrics/server/ServerMetricsSupport.h"
#include "scope/DeferContext.hpp"

#include <chrono>
#include <sstream>
#include <system_error>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace highp::metrics {
    ServerMetricsWriter::ServerMetricsWriter(
        std::shared_ptr<IServerMetrics> metrics,
        ServerMetricsConfig config,
        RunManifest manifest
    )
        : _metrics(std::move(metrics)),
          _config(std::move(config)),
          _manifest(std::move(manifest)) {
    }

    ServerMetricsWriter::~ServerMetricsWriter() noexcept {
        Stop();
    }

    bool ServerMetricsWriter::Start() {
        _lastError.clear();
        if (_running.load()) {
            return true;
        }

        _summary.Reset();
        _runtimeSampler.Reset();
        _manifest.startedAt.reset();
        _manifest.finishedAt.reset();
        _outputDirectory.clear();
        _manifestPath.clear();
        _snapshotPath.clear();
        _summaryPath.clear();

        if (!_config.enabled) {
            _outputDirectory = _config.ResolveOutputDirectory();
            _manifest.outputRoot = _config.outputRoot;
            _manifest.runId = _config.ResolveRunId();
            return true;
        }

        _manifest.outputRoot = _config.outputRoot;
        _manifest.runId = _config.ResolveRunId();
        if (_manifest.serverName.empty()) {
            _manifest.serverName = "server";
        }
        if (!_manifest.startedAt.has_value()) {
            _manifest.startedAt = std::chrono::system_clock::now();
        }

        if (!InitializeArtifacts()) {
            return false;
        }

        if (!WriteManifest()) {
            return false;
        }

        if (!CaptureAndWriteSnapshot()) {
            return false;
        }

        _running.store(true, std::memory_order_release);

        try {
            _workerThread = std::jthread([this](std::stop_token stopToken) {
                ThreadMain(stopToken);
            });
        }
        catch (const std::exception& ex) {
            _running.store(false, std::memory_order_release);
            _lastError = std::string{"failed to start metrics writer thread: "} + ex.what();
            return false;
        }
        catch (...) {
            _running.store(false, std::memory_order_release);
            _lastError = "failed to start metrics writer thread";
            return false;
        }
        return true;
    }

    void ServerMetricsWriter::Stop() {
        if (!_running.load(std::memory_order_acquire) && !_workerThread.joinable()) {
            return;
        }

        _running.store(false, std::memory_order_release);
        if (_workerThread.joinable()) {
            _workerThread.request_stop();
            _workerThread.join();
        }

        if (_config.enabled) {
            if (!_manifest.finishedAt.has_value()) {
                _manifest.finishedAt = std::chrono::system_clock::now();
            }

            CaptureAndWriteSnapshot();
            WriteManifest();

            std::ofstream summary(_summaryPath, std::ios::out | std::ios::trunc);
            if (summary.is_open()) {
                summary << _summary.BuildMarkdown(_manifest, _outputDirectory);
            }
        }
    }

    void ServerMetricsWriter::SetLogicThreadId(DWORD threadId) noexcept {
        _runtimeSampler.SetLogicThreadId(threadId);
    }

    bool ServerMetricsWriter::IsRunning() const noexcept {
        return _running.load(std::memory_order_acquire);
    }

    std::filesystem::path ServerMetricsWriter::OutputDirectory() const {
        return _outputDirectory;
    }

    std::string ServerMetricsWriter::LastError() const {
        return _lastError;
    }

    void ServerMetricsWriter::ThreadMain(const std::stop_token& st) {
        while (!st.stop_requested() && _running.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(_config.snapshotInterval);
            if (st.stop_requested() || !_running.load(std::memory_order_acquire)) {
                break;
            }

            if (!CaptureAndWriteSnapshot()) {
                break;
            }
        }

        _running.store(false, std::memory_order_release);
    }

    bool ServerMetricsWriter::InitializeArtifacts() {
        std::error_code ec;
        _outputDirectory = _manifest.OutputDirectory();
        _manifestPath = _outputDirectory / "manifest.json";
        _snapshotPath = _outputDirectory / "server-metrics.jsonl";
        _summaryPath = _outputDirectory / "summary.md";

        std::filesystem::create_directories(_outputDirectory, ec);
        if (ec) {
            _lastError = "failed to create metrics output directory: " + ec.message();
            return false;
        }

        return true;
    }

    bool ServerMetricsWriter::WriteManifest() {
        std::ofstream out(_manifestPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            _lastError = "failed to open manifest file";
            return false;
        }

        out << _manifest.ToJson();
        if (!out.good()) {
            _lastError = "failed to write manifest file";
            return false;
        }

        return true;
    }

    bool ServerMetricsWriter::CaptureAndWriteSnapshot() {
        const ServerMetricsSnapshot snapshot = _metrics->TakeSnapshot();

        SnapshotRecord record{};
        record.capturedAt = snapshot.capturedAt;
        record.acceptTotal = snapshot.acceptTotal;
        record.disconnectTotal = snapshot.disconnectTotal;
        record.recvPacketsTotal = snapshot.recvPacketsTotal;
        record.sendPacketsTotal = snapshot.sendPacketsTotal;
        record.recvBytesTotal = snapshot.recvBytesTotal;
        record.sendBytesTotal = snapshot.sendBytesTotal;
        record.sendFailTotal = snapshot.sendFailTotal;
        record.packetValidationTotal = snapshot.packetValidationTotal;
        record.packetValidationFailureTotal = snapshot.packetValidationFailureTotal;
        record.connectedClients = snapshot.connectedClients;
        record.pendingAcceptCount = snapshot.pendingAcceptCount;
        record.pendingRecvCount = snapshot.pendingRecvCount;
        record.pendingSendCount = snapshot.pendingSendCount;
        record.dispatcherQueueLength = snapshot.dispatcherQueueLength;
        record.runnableWorkerThreadCount = snapshot.runnableWorkerThreadCount;
        record.queueWaitMsAvg = internal::TimingAverageMs(snapshot.queueWait);
        record.queueWaitMsMax = internal::TimingMaxMs(snapshot.queueWait);
        record.dispatchProcessMsAvg = internal::TimingAverageMs(snapshot.dispatchProcess);
        record.dispatchProcessMsMax = internal::TimingMaxMs(snapshot.dispatchProcess);
        record.tickDurationMsAvg = internal::TimingAverageMs(snapshot.tickDuration);
        record.tickDurationMsMax = internal::TimingMaxMs(snapshot.tickDuration);
        record.tickLagMsAvg = internal::TimingAverageMs(snapshot.tickLag);
        record.tickLagMsMax = internal::TimingMaxMs(snapshot.tickLag);

        const RuntimeSample runtime = _runtimeSampler.Sample();
        record.processCpuPercent = runtime.processCpuPercent;
        record.logicThreadCpuPercent = runtime.logicThreadCpuPercent;
        record.threadCount = runtime.threadCount;

        std::ofstream out(_snapshotPath, std::ios::out | std::ios::app);
        if (!out.is_open()) {
            _lastError = "failed to open metrics snapshot file";
            return false;
        }

        out << BuildSnapshotJson(&record, &_previousRecord) << '\n';
        if (!out.good()) {
            _lastError = "failed to write metrics snapshot";
            return false;
        }

        _summary.Observe(&record, &_previousRecord);
        _previousRecord = record;
        return true;
    }

    std::string ServerMetricsWriter::BuildSnapshotJson(
        const SnapshotRecord* record,
        const SnapshotRecord* previous
    ) const {
        const std::chrono::seconds elapsed = previous == nullptr
            ? std::chrono::seconds{1}
            : std::chrono::duration_cast<std::chrono::seconds>(
                record->capturedAt - previous->capturedAt);

        const auto rate = [&](uint64_t current, uint64_t prior) {
            return previous ? ComputeRate(current, prior, elapsed) : 0.0;
        };

        std::ostringstream oss;
        JsonObjectWriter json(oss);
        json << JsonTimestamp{"ts", record->capturedAt}
             << JsonCounter{"accept_total", record->acceptTotal}
             << JsonNumber{"accept_per_sec", rate(record->acceptTotal, previous ? previous->acceptTotal : 0)}
             << JsonCounter{"disconnect_total", record->disconnectTotal}
             << JsonNumber{"disconnect_per_sec", rate(record->disconnectTotal, previous ? previous->disconnectTotal : 0)}
             << JsonCounter{"recv_packets_total", record->recvPacketsTotal}
             << JsonNumber{"recv_packets_per_sec", rate(record->recvPacketsTotal, previous ? previous->recvPacketsTotal : 0)}
             << JsonCounter{"send_packets_total", record->sendPacketsTotal}
             << JsonNumber{"send_packets_per_sec", rate(record->sendPacketsTotal, previous ? previous->sendPacketsTotal : 0)}
             << JsonCounter{"recv_bytes_total", record->recvBytesTotal}
             << JsonNumber{"recv_bytes_per_sec", rate(record->recvBytesTotal, previous ? previous->recvBytesTotal : 0)}
             << JsonCounter{"send_bytes_total", record->sendBytesTotal}
             << JsonNumber{"send_bytes_per_sec", rate(record->sendBytesTotal, previous ? previous->sendBytesTotal : 0)}
             << JsonCounter{"send_fail_total", record->sendFailTotal}
             << JsonCounter{"packet_validation_total", record->packetValidationTotal}
             << JsonCounter{"packet_validation_failure_total", record->packetValidationFailureTotal}
             << JsonGauge{"connected_clients", record->connectedClients}
             << JsonGauge{"pending_accept_count", record->pendingAcceptCount}
             << JsonGauge{"pending_recv_count", record->pendingRecvCount}
             << JsonGauge{"pending_send_count", record->pendingSendCount}
             << JsonGauge{"dispatcher_queue_length", record->dispatcherQueueLength}
             << JsonGauge{"runnable_worker_thread_count", record->runnableWorkerThreadCount}
             << JsonNumber{"queue_wait_ms_avg", record->queueWaitMsAvg}
             << JsonNumber{"queue_wait_ms_max", record->queueWaitMsMax}
             << JsonNumber{"dispatch_process_ms_avg", record->dispatchProcessMsAvg}
             << JsonNumber{"dispatch_process_ms_max", record->dispatchProcessMsMax}
             << JsonNumber{"tick_duration_ms_avg", record->tickDurationMsAvg}
             << JsonNumber{"tick_duration_ms_max", record->tickDurationMsMax}
             << JsonNumber{"tick_lag_ms_avg", record->tickLagMsAvg}
             << JsonNumber{"tick_lag_ms_max", record->tickLagMsMax}
             << JsonNumber{"process_cpu_percent", record->processCpuPercent}
             << JsonNumber{"logic_thread_cpu_percent", record->logicThreadCpuPercent}
             << JsonUint{"thread_count", record->threadCount};
        return oss.str();
    }

    double ServerMetricsWriter::ToPerSecond(
        uint64_t delta,
        std::chrono::nanoseconds elapsed) noexcept {
        const double seconds = std::chrono::duration<double>(elapsed).count();
        if (seconds <= 0.0) {
            return 0.0;
        }

        return static_cast<double>(delta) / seconds;
    }

    double ServerMetricsWriter::ComputeRate(
        uint64_t current,
        uint64_t prior,
        std::chrono::nanoseconds elapsed) noexcept {
        return ToPerSecond(current >= prior ? current - prior : 0, elapsed);
    }
}
