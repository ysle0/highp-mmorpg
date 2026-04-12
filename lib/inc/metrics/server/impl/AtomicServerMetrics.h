#pragma once

#include "metrics/server/IServerMetrics.h"
#include <atomic>

namespace highp::metrics {
    class AtomicServerMetrics final : public IServerMetrics {
    public:
        AtomicServerMetrics() = default;

        void OnAcceptPosted() override;
        void OnAcceptPostFailed() override;
        void OnAcceptCompleted() override;
        void OnAcceptCompletionFailed() override;

        void OnClientConnected() override;
        void OnClientDisconnected() override;

        void OnRecvPosted() override;
        void OnRecvPostFailed() override;
        void OnRecvCompleted(size_t bytesTransferred, bool countPayload) override;

        void OnSendPosted() override;
        void OnSendPostFailed() override;
        void OnSendCompleted(size_t bytesTransferred, bool countPayload) override;

        void OnWorkerDispatchBegin() override;
        void OnWorkerDispatchEnd() override;

        void OnPacketValidationSucceeded() override;
        void OnPacketValidationFailed() override;

        void OnQueuePushed(size_t count = 1) override;
        void OnQueueDrained(size_t count = 1) override;
        void ObserveQueueWait(std::chrono::nanoseconds duration) override;

        void ObserveDispatchProcess(std::chrono::nanoseconds duration) override;
        void ObserveTickDuration(std::chrono::nanoseconds duration) override;
        void ObserveTickLag(std::chrono::nanoseconds duration) override;

        [[nodiscard]] ServerMetricsSnapshot TakeSnapshot() override;

    private:
        struct TimingWindowState {
            std::atomic<uint64_t> count{0};
            std::atomic<uint64_t> totalNanoseconds{0};
            std::atomic<uint64_t> maxNanoseconds{0};
        };

        static void Increment(std::atomic<int64_t>& value, int64_t delta = 1) noexcept;
        static void Decrement(std::atomic<int64_t>& value, int64_t delta = 1) noexcept;
        static void ObserveMax(std::atomic<uint64_t>& currentMax, uint64_t value) noexcept;
        static TimingWindow DrainWindow(TimingWindowState& window) noexcept;
        static void ObserveWindow(TimingWindowState& window, uint64_t nanoseconds) noexcept;

        std::atomic<uint64_t> _acceptTotal{0};
        std::atomic<uint64_t> _disconnectTotal{0};
        std::atomic<uint64_t> _recvPacketsTotal{0};
        std::atomic<uint64_t> _sendPacketsTotal{0};
        std::atomic<uint64_t> _recvBytesTotal{0};
        std::atomic<uint64_t> _sendBytesTotal{0};
        std::atomic<uint64_t> _sendFailTotal{0};
        std::atomic<uint64_t> _packetValidationTotal{0};
        std::atomic<uint64_t> _packetValidationFailureTotal{0};

        std::atomic<int64_t> _connectedClients{0};
        std::atomic<int64_t> _pendingAcceptCount{0};
        std::atomic<int64_t> _pendingRecvCount{0};
        std::atomic<int64_t> _pendingSendCount{0};
        std::atomic<int64_t> _dispatcherQueueLength{0};
        std::atomic<int64_t> _runnableWorkerThreadCount{0};

        TimingWindowState _queueWait;
        TimingWindowState _dispatchProcess;
        TimingWindowState _tickDuration;
        TimingWindowState _tickLag;
    };
} // namespace highp::metrics
