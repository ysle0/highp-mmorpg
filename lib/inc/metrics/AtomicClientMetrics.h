#pragma once

#include "metrics/IClientMetrics.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace highp::metrics {
    class AtomicClientMetrics final : public IClientMetrics {
    public:
        AtomicClientMetrics() = default;

        void ObserveConnectLatency(std::chrono::nanoseconds duration, bool success) override;
        void OnConnected() override;
        void OnDisconnected(ClientDisconnectReason reason) override;

        void OnSendCompleted(size_t bytesTransferred) override;
        void OnSendFailed() override;
        void OnRecvCompleted(size_t bytesTransferred) override;

        void OnPacketValidationSucceeded() override;
        void OnPacketValidationFailed() override;

        void TrackRequest(
            uint32_t sequence,
            int32_t responseType,
            std::chrono::milliseconds timeout) override;
        void ResolveRequest(
            uint32_t sequence,
            int32_t receivedType) override;

        [[nodiscard]] ClientMetricsSnapshot TakeSnapshot() const override;

    private:
        struct TimingWindowState {
            std::atomic<uint64_t> count{0};
            std::atomic<uint64_t> totalNanoseconds{0};
            std::atomic<uint64_t> maxNanoseconds{0};
        };

        struct PendingRequest {
            int32_t responseType = 0;
            uint64_t sentAtNs = 0;
            uint64_t deadlineNs = 0;
        };

        static void Increment(std::atomic<int64_t>& value, int64_t delta = 1) noexcept;
        static void ObserveMax(std::atomic<uint64_t>& currentMax, uint64_t value) noexcept;
        static void ObserveWindow(TimingWindowState& window, uint64_t nanoseconds) noexcept;
        static TimingWindow DrainWindow(TimingWindowState& window) noexcept;
        static uint64_t NowSteadyNanoseconds() noexcept;

        void ObserveDisconnectReason(ClientDisconnectReason reason) noexcept;
        void ExpireTimedOutRequests(uint64_t nowNs) const;

        std::atomic<uint64_t> _connectSuccessTotal{0};
        std::atomic<uint64_t> _connectFailureTotal{0};
        std::atomic<uint64_t> _disconnectTotal{0};
        std::atomic<uint64_t> _sendPacketsTotal{0};
        std::atomic<uint64_t> _recvPacketsTotal{0};
        std::atomic<uint64_t> _sendBytesTotal{0};
        std::atomic<uint64_t> _recvBytesTotal{0};
        std::atomic<uint64_t> _sendFailTotal{0};
        mutable std::atomic<uint64_t> _responseTimeoutTotal{0};
        std::atomic<uint64_t> _packetValidationTotal{0};
        std::atomic<uint64_t> _packetValidationFailureTotal{0};

        std::atomic<uint64_t> _disconnectTimeoutTotal{0};
        std::atomic<uint64_t> _disconnectServerCloseTotal{0};
        std::atomic<uint64_t> _disconnectRecvFailTotal{0};
        std::atomic<uint64_t> _disconnectLocalCloseTotal{0};

        std::atomic<int64_t> _connected{0};
        std::atomic<uint64_t> _lastConnectLatencyNs{0};
        std::atomic<uint64_t> _sessionStartedAtNs{0};
        std::atomic<uint64_t> _lastSessionUptimeNs{0};
        std::atomic<uint64_t> _maxSessionUptimeNs{0};
        std::atomic<uint64_t> _lastRecvAtNs{0};
        std::atomic<uint8_t> _lastDisconnectReason{
            static_cast<uint8_t>(ClientDisconnectReason::None)
        };

        mutable TimingWindowState _connectLatency;
        mutable TimingWindowState _requestRtt;

        mutable std::mutex _pendingMutex;
        mutable std::unordered_map<uint32_t, PendingRequest> _pendingRequests;
    };
} // namespace highp::metrics
