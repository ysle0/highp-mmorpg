#include "pch.h"

#include "metrics/server/impl/AtomicServerMetrics.h"

namespace highp::metrics {
    void AtomicServerMetrics::Increment(std::atomic<int64_t>& value, int64_t delta) noexcept {
        value.fetch_add(delta, std::memory_order_relaxed);
    }

    void AtomicServerMetrics::Decrement(std::atomic<int64_t>& value, int64_t delta) noexcept {
        value.fetch_sub(delta, std::memory_order_relaxed);
    }

    void AtomicServerMetrics::ObserveMax(std::atomic<uint64_t>& currentMax, uint64_t value) noexcept {
        if (value > currentMax.load(std::memory_order_relaxed)) {
            currentMax.store(value, std::memory_order_relaxed);
        }
    }

    void AtomicServerMetrics::ObserveWindow(TimingWindowState& window, uint64_t nanoseconds) noexcept {
        window.count.fetch_add(1, std::memory_order_relaxed);
        window.totalNanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
        ObserveMax(window.maxNanoseconds, nanoseconds);
    }

    TimingWindow AtomicServerMetrics::DrainWindow(TimingWindowState& window) noexcept {
        TimingWindow result{};
        result.count = window.count.exchange(0, std::memory_order_acq_rel);
        result.totalNanoseconds = window.totalNanoseconds.exchange(0, std::memory_order_acq_rel);
        result.maxNanoseconds = window.maxNanoseconds.exchange(0, std::memory_order_acq_rel);
        return result;
    }

    void AtomicServerMetrics::OnAcceptPosted() {
        Increment(_pendingAcceptCount);
    }

    void AtomicServerMetrics::OnAcceptPostFailed() {
    }

    void AtomicServerMetrics::OnAcceptCompleted() {
        Decrement(_pendingAcceptCount);
        _acceptTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicServerMetrics::OnAcceptCompletionFailed() {
        Decrement(_pendingAcceptCount);
    }

    void AtomicServerMetrics::OnClientConnected() {
        Increment(_connectedClients);
    }

    void AtomicServerMetrics::OnClientDisconnected() {
        Decrement(_connectedClients);
        _disconnectTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicServerMetrics::OnRecvPosted() {
        Increment(_pendingRecvCount);
    }

    void AtomicServerMetrics::OnRecvPostFailed() {
    }

    void AtomicServerMetrics::OnRecvCompleted(size_t bytesTransferred, bool countPayload) {
        Decrement(_pendingRecvCount);
        if (!countPayload) {
            return;
        }

        _recvPacketsTotal.fetch_add(1, std::memory_order_relaxed);
        _recvBytesTotal.fetch_add(static_cast<uint64_t>(bytesTransferred), std::memory_order_relaxed);
    }

    void AtomicServerMetrics::OnSendPosted() {
        Increment(_pendingSendCount);
    }

    void AtomicServerMetrics::OnSendPostFailed() {
        _sendFailTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicServerMetrics::OnSendCompleted(size_t bytesTransferred, bool countPayload) {
        Decrement(_pendingSendCount);
        if (!countPayload) {
            return;
        }

        _sendPacketsTotal.fetch_add(1, std::memory_order_relaxed);
        _sendBytesTotal.fetch_add(static_cast<uint64_t>(bytesTransferred), std::memory_order_relaxed);
    }

    void AtomicServerMetrics::OnWorkerDispatchBegin() {
        Increment(_runnableWorkerThreadCount);
    }

    void AtomicServerMetrics::OnWorkerDispatchEnd() {
        Decrement(_runnableWorkerThreadCount);
    }

    void AtomicServerMetrics::OnPacketValidationSucceeded() {
        _packetValidationTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicServerMetrics::OnPacketValidationFailed() {
        _packetValidationTotal.fetch_add(1, std::memory_order_relaxed);
        _packetValidationFailureTotal.fetch_add(1, std::memory_order_relaxed);
    }

    void AtomicServerMetrics::OnQueuePushed(size_t count) {
        Increment(_dispatcherQueueLength, static_cast<int64_t>(count));
    }

    void AtomicServerMetrics::OnQueueDrained(size_t count) {
        Decrement(_dispatcherQueueLength, static_cast<int64_t>(count));
    }

    void AtomicServerMetrics::ObserveQueueWait(std::chrono::nanoseconds duration) {
        if (duration.count() < 0) {
            return;
        }
        ObserveWindow(_queueWait, static_cast<uint64_t>(duration.count()));
    }

    void AtomicServerMetrics::ObserveDispatchProcess(std::chrono::nanoseconds duration) {
        if (duration.count() < 0) {
            return;
        }
        ObserveWindow(_dispatchProcess, static_cast<uint64_t>(duration.count()));
    }

    void AtomicServerMetrics::ObserveTickDuration(std::chrono::nanoseconds duration) {
        if (duration.count() < 0) {
            return;
        }
        ObserveWindow(_tickDuration, static_cast<uint64_t>(duration.count()));
    }

    void AtomicServerMetrics::ObserveTickLag(std::chrono::nanoseconds duration) {
        if (duration.count() < 0) {
            return;
        }
        ObserveWindow(_tickLag, static_cast<uint64_t>(duration.count()));
    }

    ServerMetricsSnapshot AtomicServerMetrics::TakeSnapshot() {
        ServerMetricsSnapshot snapshot{};
        snapshot.capturedAt = ServerMetricsSnapshot::Clock::now();

        snapshot.acceptTotal = _acceptTotal.load(std::memory_order_relaxed);
        snapshot.disconnectTotal = _disconnectTotal.load(std::memory_order_relaxed);
        snapshot.recvPacketsTotal = _recvPacketsTotal.load(std::memory_order_relaxed);
        snapshot.sendPacketsTotal = _sendPacketsTotal.load(std::memory_order_relaxed);
        snapshot.recvBytesTotal = _recvBytesTotal.load(std::memory_order_relaxed);
        snapshot.sendBytesTotal = _sendBytesTotal.load(std::memory_order_relaxed);
        snapshot.sendFailTotal = _sendFailTotal.load(std::memory_order_relaxed);
        snapshot.packetValidationTotal = _packetValidationTotal.load(std::memory_order_relaxed);
        snapshot.packetValidationFailureTotal = _packetValidationFailureTotal.load(std::memory_order_relaxed);

        snapshot.connectedClients = _connectedClients.load(std::memory_order_relaxed);
        snapshot.pendingAcceptCount = _pendingAcceptCount.load(std::memory_order_relaxed);
        snapshot.pendingRecvCount = _pendingRecvCount.load(std::memory_order_relaxed);
        snapshot.pendingSendCount = _pendingSendCount.load(std::memory_order_relaxed);
        snapshot.dispatcherQueueLength = _dispatcherQueueLength.load(std::memory_order_relaxed);
        snapshot.runnableWorkerThreadCount = _runnableWorkerThreadCount.load(std::memory_order_relaxed);

        snapshot.queueWait = DrainWindow(_queueWait);
        snapshot.dispatchProcess = DrainWindow(_dispatchProcess);
        snapshot.tickDuration = DrainWindow(_tickDuration);
        snapshot.tickLag = DrainWindow(_tickLag);

        return snapshot;
    }
} // namespace highp::metrics
