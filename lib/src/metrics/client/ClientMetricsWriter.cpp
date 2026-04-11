#include "pch.h"

#include "metrics/client/ClientMetricsWriter.h"
#include "metrics/client/impl/NoopClientMetrics.h"
#include "src/metrics/server/ServerMetricsSupport.h"

#include <algorithm>
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
    constexpr auto kDefaultSnapshotInterval = std::chrono::milliseconds{1000};
}

namespace highp::metrics {
    ClientMetricsWriter::ClientMetricsWriter(
        std::shared_ptr<IClientMetrics> metrics,
        ClientMetricsConfig config,
        RunManifest manifest)
        : _metrics(std::move(metrics)),
          _config(std::move(config)),
          _manifest(std::move(manifest)) {
        if (!_metrics) {
            _metrics = std::make_shared<NoopClientMetrics>();
        }

        const ClientMetricsConfig defaultConfig{};
        if (_config.enabled == defaultConfig.enabled &&
            _config.outputRoot == defaultConfig.outputRoot &&
            _config.runId.empty() &&
            _config.snapshotInterval == defaultConfig.snapshotInterval &&
            _config.responseTimeout == defaultConfig.responseTimeout) {
            _config = ClientMetricsConfig::FromEnvironment();
        }

        if (_config.snapshotInterval.count() <= 0) {
            _config.snapshotInterval = kDefaultSnapshotInterval;
        }
    }

    ClientMetricsWriter::~ClientMetricsWriter() noexcept {
        Stop();
    }

    bool ClientMetricsWriter::Start() {
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
        _hasRuntimeBaseline = false;
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
            _manifest.serverName = "client";
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
        } catch (const std::exception& ex) {
            _running.store(false, std::memory_order_release);
            _lastError = std::string{"failed to start client metrics writer thread: "} + ex.what();
            return false;
        } catch (...) {
            _running.store(false, std::memory_order_release);
            _lastError = "failed to start client metrics writer thread";
            return false;
        }

        return true;
    }

    void ClientMetricsWriter::Stop() {
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

    bool ClientMetricsWriter::IsRunning() const noexcept {
        return _running.load(std::memory_order_acquire);
    }

    std::filesystem::path ClientMetricsWriter::OutputDirectory() const {
        return _outputDirectory;
    }

    std::string ClientMetricsWriter::LastError() const {
        std::lock_guard lock(_mutex);
        return _lastError;
    }

    void ClientMetricsWriter::ThreadMain(std::stop_token stopToken) {
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

    bool ClientMetricsWriter::InitializeArtifacts() {
        std::error_code ec;
        _outputDirectory = _manifest.OutputDirectory();
        _manifestPath = _outputDirectory / "manifest.json";
        _snapshotPath = _outputDirectory / "client-metrics.jsonl";
        _summaryPath = _outputDirectory / "summary.md";

        std::filesystem::create_directories(_outputDirectory, ec);
        if (ec) {
            _lastError = "failed to create client metrics output directory: " + ec.message();
            return false;
        }

        return true;
    }

    bool ClientMetricsWriter::WriteManifest() {
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

    bool ClientMetricsWriter::CaptureAndWriteSnapshot(bool finalSnapshot) {
        const ClientMetricsSnapshot snapshot = _metrics->TakeSnapshot();
        SnapshotRecord record{};
        record.capturedAt = snapshot.capturedAt;
        record.connectSuccessTotal = snapshot.connectSuccessTotal;
        record.connectFailureTotal = snapshot.connectFailureTotal;
        record.disconnectTotal = snapshot.disconnectTotal;
        record.sendPacketsTotal = snapshot.sendPacketsTotal;
        record.recvPacketsTotal = snapshot.recvPacketsTotal;
        record.sendBytesTotal = snapshot.sendBytesTotal;
        record.recvBytesTotal = snapshot.recvBytesTotal;
        record.sendFailTotal = snapshot.sendFailTotal;
        record.responseTimeoutTotal = snapshot.responseTimeoutTotal;
        record.packetValidationTotal = snapshot.packetValidationTotal;
        record.packetValidationFailureTotal = snapshot.packetValidationFailureTotal;
        record.disconnectTimeoutTotal = snapshot.disconnectTimeoutTotal;
        record.disconnectServerCloseTotal = snapshot.disconnectServerCloseTotal;
        record.disconnectRecvFailTotal = snapshot.disconnectRecvFailTotal;
        record.disconnectLocalCloseTotal = snapshot.disconnectLocalCloseTotal;
        record.connected = snapshot.connected;
        record.pendingRequestCount = snapshot.pendingRequestCount;
        record.connectLatencyMsAvg = internal::TimingAverageMs(snapshot.connectLatency);
        record.connectLatencyMsMax = internal::TimingMaxMs(snapshot.connectLatency);
        record.requestRttMsAvg = internal::TimingAverageMs(snapshot.requestRtt);
        record.requestRttMsMax = internal::TimingMaxMs(snapshot.requestRtt);
        record.lastConnectLatencyMs = snapshot.lastConnectLatencyMs;
        record.currentSessionUptimeMs = snapshot.currentSessionUptimeMs;
        record.lastSessionUptimeMs = snapshot.lastSessionUptimeMs;
        record.maxSessionUptimeMs = snapshot.maxSessionUptimeMs;
        record.timeSinceLastRecvMs = snapshot.timeSinceLastRecvMs;
        record.lastDisconnectReason = snapshot.lastDisconnectReason;

        const RuntimeSample runtime = SampleRuntime();
        record.processCpuPercent = runtime.processCpuPercent;
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
            _lastError = "failed to open client metrics snapshot file";
            return false;
        }

        const SnapshotRecord* previous = _hasPreviousRecord ? &_previousRecord : nullptr;
        out << BuildSnapshotJson(record, previous) << '\n';
        if (!out.good()) {
            _lastError = "failed to write client metrics snapshot";
            return false;
        }

        ObserveSummary(record, previous);
        _previousRecord = record;
        _hasPreviousRecord = true;
        return true;
    }

    ClientMetricsWriter::RuntimeSample ClientMetricsWriter::SampleRuntime() {
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
            return sample;
        }

        const uint64_t processKernel100ns = internal::FileTimeToUint64(kernel);
        const uint64_t processUser100ns = internal::FileTimeToUint64(user);

        if (!_hasRuntimeBaseline) {
            _hasRuntimeBaseline = true;
            _lastRuntimeSampleAt = now;
            _lastProcessKernel100ns = processKernel100ns;
            _lastProcessUser100ns = processUser100ns;
            sample.threadCount = internal::CountProcessThreads();
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
                (processSeconds / elapsedSeconds) /
                static_cast<double>(internal::GetProcessorCount()) * 100.0;
        }

        _lastRuntimeSampleAt = now;
        _lastProcessKernel100ns = processKernel100ns;
        _lastProcessUser100ns = processUser100ns;
        sample.threadCount = internal::CountProcessThreads();
        return sample;
    }

    std::string ClientMetricsWriter::BuildSnapshotJson(
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
        oss << "\"connect_success_total\":" << record.connectSuccessTotal << ",";
        oss << "\"connect_failure_total\":" << record.connectFailureTotal << ",";
        oss << "\"disconnect_total\":" << record.disconnectTotal << ",";
        oss << "\"send_packets_total\":" << record.sendPacketsTotal << ",";
        oss << "\"send_packets_per_sec\":" << internal::FormatNumber(
            previous == nullptr ? 0.0 : rate(record.sendPacketsTotal, previous->sendPacketsTotal)) << ",";
        oss << "\"recv_packets_total\":" << record.recvPacketsTotal << ",";
        oss << "\"recv_packets_per_sec\":" << internal::FormatNumber(
            previous == nullptr ? 0.0 : rate(record.recvPacketsTotal, previous->recvPacketsTotal)) << ",";
        oss << "\"send_bytes_total\":" << record.sendBytesTotal << ",";
        oss << "\"send_bytes_per_sec\":" << internal::FormatNumber(
            previous == nullptr ? 0.0 : rate(record.sendBytesTotal, previous->sendBytesTotal)) << ",";
        oss << "\"recv_bytes_total\":" << record.recvBytesTotal << ",";
        oss << "\"recv_bytes_per_sec\":" << internal::FormatNumber(
            previous == nullptr ? 0.0 : rate(record.recvBytesTotal, previous->recvBytesTotal)) << ",";
        oss << "\"send_fail_total\":" << record.sendFailTotal << ",";
        oss << "\"response_timeout_total\":" << record.responseTimeoutTotal << ",";
        oss << "\"packet_validation_total\":" << record.packetValidationTotal << ",";
        oss << "\"packet_validation_failure_total\":" << record.packetValidationFailureTotal << ",";
        oss << "\"disconnect_timeout_total\":" << record.disconnectTimeoutTotal << ",";
        oss << "\"disconnect_server_close_total\":" << record.disconnectServerCloseTotal << ",";
        oss << "\"disconnect_recv_fail_total\":" << record.disconnectRecvFailTotal << ",";
        oss << "\"disconnect_local_close_total\":" << record.disconnectLocalCloseTotal << ",";
        oss << "\"connected\":" << record.connected << ",";
        oss << "\"pending_request_count\":" << record.pendingRequestCount << ",";
        oss << "\"connect_latency_ms_last\":" << internal::FormatNumber(record.lastConnectLatencyMs) << ",";
        oss << "\"connect_latency_ms_avg\":" << internal::FormatNumber(record.connectLatencyMsAvg) << ",";
        oss << "\"connect_latency_ms_max\":" << internal::FormatNumber(record.connectLatencyMsMax) << ",";
        oss << "\"request_rtt_ms_avg\":" << internal::FormatNumber(record.requestRttMsAvg) << ",";
        oss << "\"request_rtt_ms_max\":" << internal::FormatNumber(record.requestRttMsMax) << ",";
        oss << "\"current_session_uptime_ms\":" << record.currentSessionUptimeMs << ",";
        oss << "\"last_session_uptime_ms\":" << record.lastSessionUptimeMs << ",";
        oss << "\"max_session_uptime_ms\":" << record.maxSessionUptimeMs << ",";
        oss << "\"time_since_last_recv_ms\":" << record.timeSinceLastRecvMs << ",";
        oss << "\"last_disconnect_reason\":\""
            << internal::JsonEscape(std::string{ToString(record.lastDisconnectReason)}) << "\",";
        oss << "\"process_cpu_percent\":" << internal::FormatNumber(record.processCpuPercent) << ",";
        oss << "\"thread_count\":" << record.threadCount;
        oss << "}";
        return oss.str();
    }

    void ClientMetricsWriter::ObserveSummary(const SnapshotRecord& record, const SnapshotRecord* previous) {
        ++_summary.snapshots;
        _summary.avgConnectLatencyMsSum += record.connectLatencyMsAvg;
        _summary.avgRequestRttMsSum += record.requestRttMsAvg;
        _summary.maxConnectLatencyMs = std::max(_summary.maxConnectLatencyMs, record.connectLatencyMsMax);
        _summary.maxRequestRttMs = std::max(_summary.maxRequestRttMs, record.requestRttMsMax);
        _summary.maxSessionUptimeMs = std::max(_summary.maxSessionUptimeMs, record.maxSessionUptimeMs);
        _summary.maxTimeSinceLastRecvMs = std::max(_summary.maxTimeSinceLastRecvMs, record.timeSinceLastRecvMs);
        _summary.maxPendingRequestCount = std::max(_summary.maxPendingRequestCount, record.pendingRequestCount);
        _summary.maxProcessCpuPercent = std::max(_summary.maxProcessCpuPercent, record.processCpuPercent);
        _summary.maxThreadCount = std::max(_summary.maxThreadCount, record.threadCount);
        _summary.processCpuPercentSum += record.processCpuPercent;
        _summary.connectSuccessTotal = record.connectSuccessTotal;
        _summary.connectFailureTotal = record.connectFailureTotal;
        _summary.disconnectTotal = record.disconnectTotal;
        _summary.sendFailTotal = record.sendFailTotal;
        _summary.responseTimeoutTotal = record.responseTimeoutTotal;
        _summary.packetValidationFailureTotal = record.packetValidationFailureTotal;
        _summary.disconnectTimeoutTotal = record.disconnectTimeoutTotal;
        _summary.disconnectServerCloseTotal = record.disconnectServerCloseTotal;
        _summary.disconnectRecvFailTotal = record.disconnectRecvFailTotal;
        _summary.disconnectLocalCloseTotal = record.disconnectLocalCloseTotal;
        _summary.lastDisconnectReason = record.lastDisconnectReason;

        const auto elapsed = previous == nullptr
                                 ? std::chrono::seconds{1}
                                 : std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     record.capturedAt - previous->capturedAt);
        const auto rate = [&](uint64_t current, uint64_t prior) {
            return ToPerSecond(current >= prior ? current - prior : 0, elapsed);
        };

        const double sendPacketsPerSec = previous == nullptr
                                             ? 0.0
                                             : rate(record.sendPacketsTotal, previous->sendPacketsTotal);
        const double recvPacketsPerSec = previous == nullptr
                                             ? 0.0
                                             : rate(record.recvPacketsTotal, previous->recvPacketsTotal);
        const double sendBytesPerSec = previous == nullptr
                                           ? 0.0
                                           : rate(record.sendBytesTotal, previous->sendBytesTotal);
        const double recvBytesPerSec = previous == nullptr
                                           ? 0.0
                                           : rate(record.recvBytesTotal, previous->recvBytesTotal);

        _summary.sendPacketsPerSecSum += sendPacketsPerSec;
        _summary.recvPacketsPerSecSum += recvPacketsPerSec;
        _summary.sendBytesPerSecSum += sendBytesPerSec;
        _summary.recvBytesPerSecSum += recvBytesPerSec;
        _summary.maxSendPacketsPerSec = std::max(_summary.maxSendPacketsPerSec, sendPacketsPerSec);
        _summary.maxRecvPacketsPerSec = std::max(_summary.maxRecvPacketsPerSec, recvPacketsPerSec);
        _summary.maxSendBytesPerSec = std::max(_summary.maxSendBytesPerSec, sendBytesPerSec);
        _summary.maxRecvBytesPerSec = std::max(_summary.maxRecvBytesPerSec, recvBytesPerSec);
    }

    std::string ClientMetricsWriter::BuildSummaryMarkdown() const {
        const double snapshotCount = _summary.snapshots == 0 ? 1.0 : static_cast<double>(_summary.snapshots);
        const auto avg = [&](double sum) {
            return sum / snapshotCount;
        };

        std::ostringstream oss;
        oss << "Client Metrics Summary\n\n";
        oss << "- run_id: " << (_manifest.runId.empty() ? "run" : _manifest.runId) << "\n";
        oss << "- client_name: " << (_manifest.serverName.empty() ? "client" : _manifest.serverName) << "\n";
        oss << "- output_dir: " << _outputDirectory.string() << "\n";
        oss << "- started_at: " << (_manifest.startedAt ? internal::FormatUtcTime(*_manifest.startedAt) : std::string{}) << "\n";
        oss << "- finished_at: " << (_manifest.finishedAt ? internal::FormatUtcTime(*_manifest.finishedAt) : std::string{}) << "\n\n";

        oss << "Core Results\n";
        oss << "- connect success total: " << _summary.connectSuccessTotal << "\n";
        oss << "- connect failure total: " << _summary.connectFailureTotal << "\n";
        oss << "- avg connect latency ms: " << internal::FormatNumber(avg(_summary.avgConnectLatencyMsSum)) << "\n";
        oss << "- max connect latency ms: " << internal::FormatNumber(_summary.maxConnectLatencyMs) << "\n";
        oss << "- avg request RTT ms: " << internal::FormatNumber(avg(_summary.avgRequestRttMsSum)) << "\n";
        oss << "- max request RTT ms: " << internal::FormatNumber(_summary.maxRequestRttMs) << "\n";
        oss << "- avg send packets/sec: " << internal::FormatNumber(avg(_summary.sendPacketsPerSecSum)) << "\n";
        oss << "- max send packets/sec: " << internal::FormatNumber(_summary.maxSendPacketsPerSec) << "\n";
        oss << "- avg recv packets/sec: " << internal::FormatNumber(avg(_summary.recvPacketsPerSecSum)) << "\n";
        oss << "- max recv packets/sec: " << internal::FormatNumber(_summary.maxRecvPacketsPerSec) << "\n";
        oss << "- avg send bytes/sec: " << internal::FormatNumber(avg(_summary.sendBytesPerSecSum)) << "\n";
        oss << "- max send bytes/sec: " << internal::FormatNumber(_summary.maxSendBytesPerSec) << "\n";
        oss << "- avg recv bytes/sec: " << internal::FormatNumber(avg(_summary.recvBytesPerSecSum)) << "\n";
        oss << "- max recv bytes/sec: " << internal::FormatNumber(_summary.maxRecvBytesPerSec) << "\n";
        oss << "- max session uptime ms: " << _summary.maxSessionUptimeMs << "\n";
        oss << "- max time since last recv ms: " << _summary.maxTimeSinceLastRecvMs << "\n";
        oss << "- max pending request count: " << _summary.maxPendingRequestCount << "\n";
        oss << "- send fail total: " << _summary.sendFailTotal << "\n";
        oss << "- response timeout total: " << _summary.responseTimeoutTotal << "\n";
        oss << "- packet validation failure total: " << _summary.packetValidationFailureTotal << "\n";
        oss << "- disconnect total: " << _summary.disconnectTotal << "\n";
        oss << "- disconnect reasons: timeout=" << _summary.disconnectTimeoutTotal
            << ", server close=" << _summary.disconnectServerCloseTotal
            << ", recv fail=" << _summary.disconnectRecvFailTotal
            << ", local close=" << _summary.disconnectLocalCloseTotal << "\n";
        oss << "- last disconnect reason: " << ToString(_summary.lastDisconnectReason) << "\n";
        oss << "- avg process CPU percent: " << internal::FormatNumber(avg(_summary.processCpuPercentSum)) << "\n";
        oss << "- max process CPU percent: " << internal::FormatNumber(_summary.maxProcessCpuPercent) << "\n";
        oss << "- max thread count: " << _summary.maxThreadCount << "\n";

        std::string bottleneck = "none observed";
        if (_summary.responseTimeoutTotal > 0 || _summary.maxTimeSinceLastRecvMs >= 3000) {
            bottleneck = "Response / Network";
        } else if (_summary.packetValidationFailureTotal > 0) {
            bottleneck = "Protocol";
        } else if (_summary.maxProcessCpuPercent >= 80.0) {
            bottleneck = "Runtime / CPU";
        }

        oss << "- bottleneck guess: " << bottleneck << "\n";
        return oss.str();
    }

    double ClientMetricsWriter::ToPerSecond(
        uint64_t delta,
        std::chrono::nanoseconds elapsed) noexcept {
        const double seconds = std::chrono::duration<double>(elapsed).count();
        if (seconds <= 0.0) {
            return 0.0;
        }

        return static_cast<double>(delta) / seconds;
    }
} // namespace highp::metrics
