#include "pch.h"

#include "metrics/AtomicClientMetrics.h"

namespace highp::metrics {
    void AtomicClientMetrics::Increment(std::atomic<int64_t>& value, int64_t delta) noexcept {
        value.fetch_add(delta, std::memory_order_relaxed);
    }

    void AtomicClientMetrics::ObserveMax(std::atomic<uint64_t>& currentMax, uint64_t value) noexcept {
        uint64_t observed = currentMax.load(std::memory_order_relaxed);
        while (observed < value &&
            !currentMax.compare_exchange_weak(
                observed,
                value,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
        }
    }

    void AtomicClientMetrics::ObserveWindow(TimingWindowState& window, uint64_t nanoseconds) noexcept {
        window.count.fetch_add(1, std::memory_order_relaxed);
        window.totalNanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
        ObserveMax(window.maxNanoseconds, nanoseconds);
    }

    TimingWindow AtomicClientMetrics::DrainWindow(TimingWindowState& window) noexcept {
        TimingWindow result{};
        result.count = window.count.exchange(0, std::memory_order_acq_rel);
        result.totalNanoseconds = window.totalNanoseconds.exchange(0, std::memory_order_acq_rel);
        result.maxNanoseconds = window.maxNanoseconds.exchange(0, std::memory_order_acq_rel);
        return result;
    }

    uint64_t AtomicClientMetrics::NowSteadyNanoseconds() noexcept {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void AtomicClientMetrics::ObserveDisconnectReason(ClientDisconnectReason reason) noexcept {
        switch (reason) {
        case ClientDisconnectReason::Timeout:
            _disconnectTimeoutTotal.fetch_add(1, std::memory_order_relaxed);
            break;
        case ClientDisconnectReason::ServerClose:
            _disconnectServerCloseTotal.fetch_add(1, std::memory_order_relaxed);
            break;
        case ClientDisconnectReason::RecvFail:
            _disconnectRecvFailTotal.fetch_add(1, std::memory_order_relaxed);
            break;
        case ClientDisconnectReason::LocalClose:
            _disconnectLocalCloseTotal.fetch_add(1, std::memory_order_relaxed);
            break;
        case ClientDisconnectReason::None:
        default:
            break;
        }
    }

    void AtomicClientMetrics::ObserveConnectLatency(std::chrono::nanoseconds duration, bool success) {
        if (duration.count() < 0) {
            return;
        }

        const uint64_t durationNs = static_cast<uint64_t>(duration.count());
        ObserveWindow(_connectLatency, durationNs);
        _lastConnectLatencyNs.store(durationNs, std::memory_order_relaxed);

        if (success) {
            _connectSuccessTotal.fetch_add(1, std::memory_order_relaxed);
        } else {
            _connectFailureTotal.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void AtomicClientMetrics::OnConnected() {
        const uint64_t nowNs = NowSteadyNanoseconds();
        _connected.store(1, std::memory_order_relaxed);
        _sessionStartedAtNs.store(nowNs, std::memory_order_relaxed);
        _lastRecvAtNs.store(nowNs, std::memory_order_relaxed);
    }

    void AtomicClientMetrics::OnDisconnected(ClientDisconnectReason reason) {
        const uint64_t nowNs = NowSteadyNanoseconds();
        _connected.store(0, std::memory_order_relaxed);
        _disconnectTotal.fetch_add(1, std::memory_order_relaxed);
        _lastDisconnectReason.store(static_cast<uint8_t>(reason), std::memory_order_relaxed);
        ObserveDisconnectReason(reason);

        const uint64_t sessionStartedAtNs = _sessionStartedAtNs.exchange(0, std::memory_order_relaxed);
        if (sessionStartedAtNs == 0 || nowNs < sessionStartedAtNs) {
            return;
        }

        const uint64_t sessionUptimeNs = nowNs - sessionStartedAtNs;
        _lastSessionUptimeNs.store(sessionUptimeNs, std::memory_order_relaxed);
        ObserveMax(_maxSessionUptimeNs, sessionUptimeNs);
    }

    void AtomicClientMetrics::OnSendCompleted(size_t bytesTransferred) {
        _sendPacketsTotal.fetch_add(1, std::memory_order_relaxed);
        _sendBytesTotal.fetch_add(static_cast<uint64_t>(bytesTransferred), std::memory_order_relaxed);
    }

    void AtomicClientMetrics::OnSendFailed() {
        _sendFailTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicClientMetrics::OnRecvCompleted(size_t bytesTransferred) {
        _recvPacketsTotal.fetch_add(1, std::memory_order_relaxed);
        _recvBytesTotal.fetch_add(static_cast<uint64_t>(bytesTransferred), std::memory_order_relaxed);
        _lastRecvAtNs.store(NowSteadyNanoseconds(), std::memory_order_relaxed);
    }

    void AtomicClientMetrics::OnPacketValidationSucceeded() {
        _packetValidationTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicClientMetrics::OnPacketValidationFailed() {
        _packetValidationTotal.fetch_add(1, std::memory_order_relaxed);
        _packetValidationFailureTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicClientMetrics::TrackRequest(
        uint32_t sequence,
        int32_t responseType,
        std::chrono::milliseconds timeout) {
        if (sequence == 0 || responseType == 0) {
            return;
        }

        const uint64_t nowNs = NowSteadyNanoseconds();
        const uint64_t timeoutNs = timeout.count() > 0
                                       ? static_cast<uint64_t>(
                                           std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count())
                                       : 0;

        std::scoped_lock lock(_pendingMutex);
        _pendingRequests.insert_or_assign(sequence, PendingRequest{
            .responseType = responseType,
            .sentAtNs = nowNs,
            .deadlineNs = timeoutNs > 0 ? nowNs + timeoutNs : 0,
        });
    }

    void AtomicClientMetrics::ResolveRequest(
        uint32_t sequence,
        int32_t receivedType) {
        if (sequence == 0) {
            return;
        }

        const uint64_t nowNs = NowSteadyNanoseconds();

        std::scoped_lock lock(_pendingMutex);
        const auto found = _pendingRequests.find(sequence);
        if (found == _pendingRequests.end()) {
            return;
        }

        const bool matches = found->second.responseType == receivedType;
        if (!matches) {
            return;
        }

        if (nowNs >= found->second.sentAtNs) {
            ObserveWindow(_requestRtt, nowNs - found->second.sentAtNs);
        }
        _pendingRequests.erase(found);
    }

    void AtomicClientMetrics::ExpireTimedOutRequests(uint64_t nowNs) const {
        std::scoped_lock lock(_pendingMutex);

        for (auto it = _pendingRequests.begin(); it != _pendingRequests.end();) {
            if (it->second.deadlineNs != 0 && nowNs >= it->second.deadlineNs) {
                _responseTimeoutTotal.fetch_add(1, std::memory_order_relaxed);
                it = _pendingRequests.erase(it);
            } else {
                ++it;
            }
        }
    }

    ClientMetricsSnapshot AtomicClientMetrics::TakeSnapshot() const {
        const uint64_t nowNs = NowSteadyNanoseconds();
        ExpireTimedOutRequests(nowNs);

        ClientMetricsSnapshot snapshot{};
        snapshot.capturedAt = ClientMetricsSnapshot::Clock::now();

        snapshot.connectSuccessTotal = _connectSuccessTotal.load(std::memory_order_relaxed);
        snapshot.connectFailureTotal = _connectFailureTotal.load(std::memory_order_relaxed);
        snapshot.disconnectTotal = _disconnectTotal.load(std::memory_order_relaxed);
        snapshot.sendPacketsTotal = _sendPacketsTotal.load(std::memory_order_relaxed);
        snapshot.recvPacketsTotal = _recvPacketsTotal.load(std::memory_order_relaxed);
        snapshot.sendBytesTotal = _sendBytesTotal.load(std::memory_order_relaxed);
        snapshot.recvBytesTotal = _recvBytesTotal.load(std::memory_order_relaxed);
        snapshot.sendFailTotal = _sendFailTotal.load(std::memory_order_relaxed);
        snapshot.responseTimeoutTotal = _responseTimeoutTotal.load(std::memory_order_relaxed);
        snapshot.packetValidationTotal = _packetValidationTotal.load(std::memory_order_relaxed);
        snapshot.packetValidationFailureTotal = _packetValidationFailureTotal.load(std::memory_order_relaxed);

        snapshot.disconnectTimeoutTotal = _disconnectTimeoutTotal.load(std::memory_order_relaxed);
        snapshot.disconnectServerCloseTotal = _disconnectServerCloseTotal.load(std::memory_order_relaxed);
        snapshot.disconnectRecvFailTotal = _disconnectRecvFailTotal.load(std::memory_order_relaxed);
        snapshot.disconnectLocalCloseTotal = _disconnectLocalCloseTotal.load(std::memory_order_relaxed);

        snapshot.connected = _connected.load(std::memory_order_relaxed);
        snapshot.lastConnectLatencyMs =
            static_cast<double>(_lastConnectLatencyNs.load(std::memory_order_relaxed)) / 1'000'000.0;
        snapshot.lastSessionUptimeMs =
            _lastSessionUptimeNs.load(std::memory_order_relaxed) / 1'000'000;
        snapshot.maxSessionUptimeMs =
            _maxSessionUptimeNs.load(std::memory_order_relaxed) / 1'000'000;
        snapshot.lastDisconnectReason = static_cast<ClientDisconnectReason>(
            _lastDisconnectReason.load(std::memory_order_relaxed));

        const uint64_t sessionStartedAtNs = _sessionStartedAtNs.load(std::memory_order_relaxed);
        if (snapshot.connected > 0 && sessionStartedAtNs > 0 && nowNs >= sessionStartedAtNs) {
            snapshot.currentSessionUptimeMs = (nowNs - sessionStartedAtNs) / 1'000'000;
        }

        const uint64_t lastRecvAtNs = _lastRecvAtNs.load(std::memory_order_relaxed);
        if (lastRecvAtNs > 0 && nowNs >= lastRecvAtNs) {
            snapshot.timeSinceLastRecvMs = (nowNs - lastRecvAtNs) / 1'000'000;
        }

        snapshot.connectLatency = DrainWindow(_connectLatency);
        snapshot.requestRtt = DrainWindow(_requestRtt);

        {
            std::scoped_lock lock(_pendingMutex);
            snapshot.pendingRequestCount = static_cast<uint32_t>(_pendingRequests.size());
        }

        return snapshot;
    }
} // namespace highp::metrics
