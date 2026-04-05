#include "pch.h"

#include "metrics/NoopServerMetrics.h"
#include "metrics/ServerMetricsWriter.h"
#include "src/metrics/ServerMetricsSupport.h"
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
namespace {
    constexpr auto K_DEFAULT_SNAPSHOT_INTERVAL = std::chrono::milliseconds{1000};
}


namespace highp::metrics {
    ServerMetricsWriter::ServerMetricsWriter(
        std::shared_ptr<IServerMetrics> metrics,
        ServerMetricsConfig config,
        RunManifest manifest)
        : _metrics(std::move(metrics)),
          _config(std::move(config)),
          _manifest(std::move(manifest)) {
        if (!_metrics) {
            _metrics = std::make_shared<NoopServerMetrics>();
        }

        const ServerMetricsConfig defaultConfig{};
        if (_config.enabled == defaultConfig.enabled &&
            _config.outputRoot == defaultConfig.outputRoot &&
            _config.runId.empty() &&
            _config.snapshotInterval == defaultConfig.snapshotInterval) {
            _config = ServerMetricsConfig::FromEnvironment();
        }

        if (_config.snapshotInterval.count() <= 0) {
            _config.snapshotInterval = K_DEFAULT_SNAPSHOT_INTERVAL;
        }
    }

    ServerMetricsWriter::~ServerMetricsWriter() noexcept {
        Stop();
    }

    bool ServerMetricsWriter::Start() {
        std::lock_guard lock(_mutex);
        _lastError.clear();
        if (_running.load(std::memory_order_acquire)) {
            return true;
        }

        _previousRecord = {};
        _hasPreviousRecord = false;
        _summary = {};
        _lastRuntimeSampleAt = {};
        _lastProcessKernel100ns = 0;
        _lastProcessUser100ns = 0;
        _lastLogicThreadKernel100ns = 0;
        _lastLogicThreadUser100ns = 0;
        _hasRuntimeBaseline = false;
        _lastLogicThreadIdSampled = 0;
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

        if (!CaptureAndWriteSnapshot(false)) {
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
        std::lock_guard lock(_mutex);
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

            [[maybe_unused]] const bool finalSnapshotWritten = CaptureAndWriteSnapshot(true);
            [[maybe_unused]] const bool manifestWritten = WriteManifest();
            std::ofstream summary(_summaryPath, std::ios::out | std::ios::trunc);
            if (summary.is_open()) {
                summary << BuildSummaryMarkdown();
            }
        }
    }

    void ServerMetricsWriter::SetLogicThreadId(DWORD threadId) noexcept {
        _logicThreadId.store(threadId, std::memory_order_release);
    }

    bool ServerMetricsWriter::IsRunning() const noexcept {
        return _running.load(std::memory_order_acquire);
    }

    std::filesystem::path ServerMetricsWriter::OutputDirectory() const {
        return _outputDirectory;
    }

    std::string ServerMetricsWriter::LastError() const {
        std::lock_guard lock(_mutex);
        return _lastError;
    }

    void ServerMetricsWriter::ThreadMain(std::stop_token stopToken) {
        while (!stopToken.stop_requested() && _running.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(_config.snapshotInterval);
            if (stopToken.stop_requested() || !_running.load(std::memory_order_acquire)) {
                break;
            }

            if (!CaptureAndWriteSnapshot(false)) {
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

    bool ServerMetricsWriter::CaptureAndWriteSnapshot(bool finalSnapshot) {
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

        const RuntimeSample runtime = SampleRuntime();
        record.processCpuPercent = runtime.processCpuPercent;
        record.logicThreadCpuPercent = runtime.logicThreadCpuPercent;
        record.threadCount = runtime.threadCount;

        if (!_hasPreviousRecord) {
            _previousRecord = record;
            _hasPreviousRecord = true;
            if (!finalSnapshot) {
                return true;
            }
        }

        std::ofstream out(_snapshotPath, std::ios::out | std::ios::app);
        if (!out.is_open()) {
            _lastError = "failed to open metrics snapshot file";
            return false;
        }

        const SnapshotRecord* previous = _hasPreviousRecord ? &_previousRecord : nullptr;
        if (previous != nullptr && previous == &record) {
            previous = nullptr;
        }

        out << BuildSnapshotJson(record, previous) << '\n';
        if (!out.good()) {
            _lastError = "failed to write metrics snapshot";
            return false;
        }

        ObserveSummary(record, previous);
        _previousRecord = record;
        _hasPreviousRecord = true;
        return true;
    }

    ServerMetricsWriter::RuntimeSample ServerMetricsWriter::SampleRuntime() {
        RuntimeSample sample{};
        const auto now = std::chrono::steady_clock::now();

        FILETIME creation{};
        FILETIME exitTime{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exitTime, &kernel, &user)) {
            _hasRuntimeBaseline = false;
            _lastRuntimeSampleAt = now;
            _lastProcessKernel100ns = 0;
            _lastProcessUser100ns = 0;
            _lastLogicThreadKernel100ns = 0;
            _lastLogicThreadUser100ns = 0;
            _lastLogicThreadIdSampled = 0;
            return sample;
        }

        const uint64_t processKernel100ns = internal::FileTimeToUint64(kernel);
        const uint64_t processUser100ns = internal::FileTimeToUint64(user);
        const DWORD logicThreadId = _logicThreadId.load(std::memory_order_acquire);

        uint64_t logicKernel100ns = 0;
        uint64_t logicUser100ns = 0;
        bool logicThreadSampled = false;
        if (logicThreadId != 0) {
            HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, logicThreadId);
            if (thread != nullptr) {
                highp::scope::DeferContext<HANDLE> threadCleanup(&thread, [](HANDLE* handle) {
                    if (*handle != nullptr && *handle != INVALID_HANDLE_VALUE) {
                        CloseHandle(*handle);
                    }
                });

                FILETIME threadCreation{};
                FILETIME threadExit{};
                FILETIME threadKernel{};
                FILETIME threadUser{};
                if (GetThreadTimes(thread, &threadCreation, &threadExit, &threadKernel, &threadUser)) {
                    logicKernel100ns = internal::FileTimeToUint64(threadKernel);
                    logicUser100ns = internal::FileTimeToUint64(threadUser);
                    logicThreadSampled = true;
                }
            }
        }

        const uint32_t processorCount = internal::GetProcessorCount();
        if (!_hasRuntimeBaseline) {
            _hasRuntimeBaseline = true;
            _lastRuntimeSampleAt = now;
            _lastProcessKernel100ns = processKernel100ns;
            _lastProcessUser100ns = processUser100ns;
            _lastLogicThreadKernel100ns = logicKernel100ns;
            _lastLogicThreadUser100ns = logicUser100ns;
            _lastLogicThreadIdSampled = logicThreadId;
            return sample;
        }

        const double elapsedSeconds =
            std::chrono::duration<double>(now - _lastRuntimeSampleAt).count();
        if (elapsedSeconds > 0.0) {
            const uint64_t processDelta =
                (processKernel100ns - _lastProcessKernel100ns) +
                (processUser100ns - _lastProcessUser100ns);
            const double processSeconds = static_cast<double>(processDelta) * 1e-7;
            sample.processCpuPercent =
                (processSeconds / elapsedSeconds) / static_cast<double>(processorCount) * 100.0;

            if (logicThreadId != 0 && logicThreadSampled) {
                if (_lastLogicThreadIdSampled != logicThreadId) {
                    _lastLogicThreadKernel100ns = logicKernel100ns;
                    _lastLogicThreadUser100ns = logicUser100ns;
                    _lastLogicThreadIdSampled = logicThreadId;
                    sample.logicThreadCpuPercent = 0.0;
                }
                else {
                    const uint64_t logicDelta =
                        (logicKernel100ns - _lastLogicThreadKernel100ns) +
                        (logicUser100ns - _lastLogicThreadUser100ns);
                    const double logicSeconds = static_cast<double>(logicDelta) * 1e-7;
                    sample.logicThreadCpuPercent =
                        (logicSeconds / elapsedSeconds) * 100.0;
                }
            }
            else {
                sample.logicThreadCpuPercent = 0.0;
            }
        }

        _lastRuntimeSampleAt = now;
        _lastProcessKernel100ns = processKernel100ns;
        _lastProcessUser100ns = processUser100ns;
        if (logicThreadSampled) {
            _lastLogicThreadKernel100ns = logicKernel100ns;
            _lastLogicThreadUser100ns = logicUser100ns;
            _lastLogicThreadIdSampled = logicThreadId;
        }

        sample.threadCount = internal::CountProcessThreads();
        return sample;
    }

    std::string ServerMetricsWriter::BuildSnapshotJson(
        const SnapshotRecord& record,
        const SnapshotRecord* previous) const {
        const auto elapsed = previous == nullptr
                                 ? std::chrono::seconds{1}
                                 : std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     record.capturedAt - previous->capturedAt);

        const auto rate = [&](uint64_t current, uint64_t prior) {
            return ToPerSecond(current >= prior ? current - prior : 0, elapsed);
        };

        std::ostringstream oss;
        oss << "{";
        oss << "\"ts\":\"" << internal::JsonEscape(internal::FormatUtcTime(record.capturedAt)) << "\",";
        oss << "\"accept_total\":" << record.acceptTotal << ",";
        oss << "\"accept_per_sec\":" << internal::FormatNumber(previous == nullptr
                                                         ? 0.0
                                                         : rate(record.acceptTotal, previous->acceptTotal)) << ",";
        oss << "\"disconnect_total\":" << record.disconnectTotal << ",";
        oss << "\"disconnect_per_sec\":" << internal::FormatNumber(previous == nullptr
                                                             ? 0.0
                                                             : rate(record.disconnectTotal, previous->disconnectTotal))
            << ",";
        oss << "\"recv_packets_total\":" << record.recvPacketsTotal << ",";
        oss << "\"recv_packets_per_sec\":" << internal::FormatNumber(
            previous == nullptr ? 0.0 : rate(record.recvPacketsTotal, previous->recvPacketsTotal)) << ",";
        oss << "\"send_packets_total\":" << record.sendPacketsTotal << ",";
        oss << "\"send_packets_per_sec\":" << internal::FormatNumber(
            previous == nullptr ? 0.0 : rate(record.sendPacketsTotal, previous->sendPacketsTotal)) << ",";
        oss << "\"recv_bytes_total\":" << record.recvBytesTotal << ",";
        oss << "\"recv_bytes_per_sec\":" << internal::FormatNumber(previous == nullptr
                                                             ? 0.0
                                                             : rate(record.recvBytesTotal, previous->recvBytesTotal)) <<
            ",";
        oss << "\"send_bytes_total\":" << record.sendBytesTotal << ",";
        oss << "\"send_bytes_per_sec\":" << internal::FormatNumber(previous == nullptr
                                                             ? 0.0
                                                             : rate(record.sendBytesTotal, previous->sendBytesTotal)) <<
            ",";
        oss << "\"send_fail_total\":" << record.sendFailTotal << ",";
        oss << "\"packet_validation_total\":" << record.packetValidationTotal << ",";
        oss << "\"packet_validation_failure_total\":" << record.packetValidationFailureTotal << ",";
        oss << "\"connected_clients\":" << record.connectedClients << ",";
        oss << "\"pending_accept_count\":" << record.pendingAcceptCount << ",";
        oss << "\"pending_recv_count\":" << record.pendingRecvCount << ",";
        oss << "\"pending_send_count\":" << record.pendingSendCount << ",";
        oss << "\"dispatcher_queue_length\":" << record.dispatcherQueueLength << ",";
        oss << "\"runnable_worker_thread_count\":" << record.runnableWorkerThreadCount << ",";
        oss << "\"queue_wait_ms_avg\":" << internal::FormatNumber(record.queueWaitMsAvg) << ",";
        oss << "\"queue_wait_ms_max\":" << internal::FormatNumber(record.queueWaitMsMax) << ",";
        oss << "\"dispatch_process_ms_avg\":" << internal::FormatNumber(record.dispatchProcessMsAvg) << ",";
        oss << "\"dispatch_process_ms_max\":" << internal::FormatNumber(record.dispatchProcessMsMax) << ",";
        oss << "\"tick_duration_ms_avg\":" << internal::FormatNumber(record.tickDurationMsAvg) << ",";
        oss << "\"tick_duration_ms_max\":" << internal::FormatNumber(record.tickDurationMsMax) << ",";
        oss << "\"tick_lag_ms_avg\":" << internal::FormatNumber(record.tickLagMsAvg) << ",";
        oss << "\"tick_lag_ms_max\":" << internal::FormatNumber(record.tickLagMsMax) << ",";
        oss << "\"process_cpu_percent\":" << internal::FormatNumber(record.processCpuPercent) << ",";
        oss << "\"logic_thread_cpu_percent\":" << internal::FormatNumber(record.logicThreadCpuPercent) << ",";
        oss << "\"thread_count\":" << record.threadCount;
        oss << "}";
        return oss.str();
    }

    void ServerMetricsWriter::ObserveSummary(
        const SnapshotRecord& record,
        const SnapshotRecord* previous) {
        ++_summary.snapshots;
        _summary.maxConnectedClients = std::max(_summary.maxConnectedClients, record.connectedClients);
        _summary.maxDispatcherQueueLength = std::max(_summary.maxDispatcherQueueLength, record.dispatcherQueueLength);
        _summary.maxPendingSendCount = std::max(_summary.maxPendingSendCount, record.pendingSendCount);
        _summary.maxQueueWaitMs = std::max(_summary.maxQueueWaitMs, record.queueWaitMsMax);
        _summary.maxDispatchProcessMs = std::max(_summary.maxDispatchProcessMs, record.dispatchProcessMsMax);
        _summary.maxTickDurationMs = std::max(_summary.maxTickDurationMs, record.tickDurationMsMax);
        _summary.maxTickLagMs = std::max(_summary.maxTickLagMs, record.tickLagMsMax);
        _summary.maxProcessCpuPercent = std::max(_summary.maxProcessCpuPercent, record.processCpuPercent);
        _summary.maxLogicThreadCpuPercent = std::max(_summary.maxLogicThreadCpuPercent, record.logicThreadCpuPercent);
        _summary.maxThreadCount = std::max(_summary.maxThreadCount, record.threadCount);
        _summary.disconnectTotal = record.disconnectTotal;
        _summary.packetValidationFailureTotal = record.packetValidationFailureTotal;

        const auto elapsed = previous == nullptr
                                 ? std::chrono::seconds{1}
                                 : std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     record.capturedAt - previous->capturedAt);

        const auto rate = [&](uint64_t current, uint64_t prior) {
            return ToPerSecond(current >= prior ? current - prior : 0, elapsed);
        };

        const double acceptPerSec = previous == nullptr ? 0.0 : rate(record.acceptTotal, previous->acceptTotal);
        const double recvPacketsPerSec = previous == nullptr
                                             ? 0.0
                                             : rate(record.recvPacketsTotal, previous->recvPacketsTotal);
        const double sendPacketsPerSec = previous == nullptr
                                             ? 0.0
                                             : rate(record.sendPacketsTotal, previous->sendPacketsTotal);

        _summary.acceptPerSecSum += acceptPerSec;
        _summary.recvPacketsPerSecSum += recvPacketsPerSec;
        _summary.sendPacketsPerSecSum += sendPacketsPerSec;
        _summary.maxAcceptPerSec = std::max(_summary.maxAcceptPerSec, acceptPerSec);
        _summary.maxRecvPacketsPerSec = std::max(_summary.maxRecvPacketsPerSec, recvPacketsPerSec);
        _summary.maxSendPacketsPerSec = std::max(_summary.maxSendPacketsPerSec, sendPacketsPerSec);

        _summary.queueWaitMsSum += record.queueWaitMsAvg;
        _summary.dispatchProcessMsSum += record.dispatchProcessMsAvg;
        _summary.tickDurationMsSum += record.tickDurationMsAvg;
        _summary.tickLagMsSum += record.tickLagMsAvg;
        _summary.processCpuPercentSum += record.processCpuPercent;
        _summary.logicThreadCpuPercentSum += record.logicThreadCpuPercent;
    }

    std::string ServerMetricsWriter::BuildSummaryMarkdown() const {
        const double snapshotCount = _summary.snapshots == 0 ? 1.0 : static_cast<double>(_summary.snapshots);
        const auto avg = [&](double sum) {
            return sum / snapshotCount;
        };

        std::ostringstream oss;
        oss << "Server Metrics Summary\n\n";
        oss << "- run_id: " << (_manifest.runId.empty() ? "run" : _manifest.runId) << "\n";
        oss << "- server_name: " << (_manifest.serverName.empty() ? "server" : _manifest.serverName) << "\n";
        oss << "- output_dir: " << _outputDirectory.string() << "\n";
        oss << "- started_at: " << (_manifest.startedAt ? internal::FormatUtcTime(*_manifest.startedAt) : std::string{}) << "\n";
        oss << "- finished_at: " << (_manifest.finishedAt ? internal::FormatUtcTime(*_manifest.finishedAt) : std::string{}) << "\n\n";

        oss << "Core Results\n";
        oss << "- max connected clients: " << _summary.maxConnectedClients << "\n";
        oss << "- avg accept/sec: " << internal::FormatNumber(avg(_summary.acceptPerSecSum)) << "\n";
        oss << "- max accept/sec: " << internal::FormatNumber(_summary.maxAcceptPerSec) << "\n";
        oss << "- avg recv packets/sec: " << internal::FormatNumber(avg(_summary.recvPacketsPerSecSum)) << "\n";
        oss << "- max recv packets/sec: " << internal::FormatNumber(_summary.maxRecvPacketsPerSec) << "\n";
        oss << "- avg send packets/sec: " << internal::FormatNumber(avg(_summary.sendPacketsPerSecSum)) << "\n";
        oss << "- max send packets/sec: " << internal::FormatNumber(_summary.maxSendPacketsPerSec) << "\n";
        oss << "- avg queue wait ms: " << internal::FormatNumber(avg(_summary.queueWaitMsSum)) << "\n";
        oss << "- max queue wait ms: " << internal::FormatNumber(_summary.maxQueueWaitMs) << "\n";
        oss << "- avg dispatch process ms: " << internal::FormatNumber(avg(_summary.dispatchProcessMsSum)) << "\n";
        oss << "- max dispatch process ms: " << internal::FormatNumber(_summary.maxDispatchProcessMs) << "\n";
        oss << "- avg tick duration ms: " << internal::FormatNumber(avg(_summary.tickDurationMsSum)) << "\n";
        oss << "- max tick duration ms: " << internal::FormatNumber(_summary.maxTickDurationMs) << "\n";
        oss << "- avg tick lag ms: " << internal::FormatNumber(avg(_summary.tickLagMsSum)) << "\n";
        oss << "- max tick lag ms: " << internal::FormatNumber(_summary.maxTickLagMs) << "\n";
        oss << "- max pending send count: " << _summary.maxPendingSendCount << "\n";
        oss << "- packet validation failure total: " << _summary.packetValidationFailureTotal << "\n";
        oss << "- disconnect total: " << _summary.disconnectTotal << "\n";
        oss << "- avg process CPU percent: " << internal::FormatNumber(avg(_summary.processCpuPercentSum)) << "\n";
        oss << "- max process CPU percent: " << internal::FormatNumber(_summary.maxProcessCpuPercent) << "\n";
        oss << "- avg logic thread CPU percent: " << internal::FormatNumber(avg(_summary.logicThreadCpuPercentSum)) << "\n";
        oss << "- max logic thread CPU percent: " << internal::FormatNumber(_summary.maxLogicThreadCpuPercent) << "\n";
        oss << "- max thread count: " << _summary.maxThreadCount << "\n";

        std::string bottleneck = "none observed";
        if (_summary.maxDispatcherQueueLength > 0 && _summary.maxTickLagMs > 0.0) {
            bottleneck = "Queue / Logic";
        }
        else if (_summary.maxPendingSendCount > 0) {
            bottleneck = "Transport / Send";
        }
        else if (_summary.maxProcessCpuPercent >= 80.0) {
            bottleneck = "Runtime / CPU";
        }

        oss << "- bottleneck guess: " << bottleneck << "\n";
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
}
