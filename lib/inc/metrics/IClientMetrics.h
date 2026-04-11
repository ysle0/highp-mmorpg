#pragma once

#include "metrics/ClientMetricsSnapshot.h"

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace highp::metrics {
    class IClientMetrics {
    public:
        virtual ~IClientMetrics() = default;

        virtual void ObserveConnectLatency(std::chrono::nanoseconds duration, bool success) = 0;
        virtual void OnConnected() = 0;
        virtual void OnDisconnected(ClientDisconnectReason reason) = 0;

        virtual void OnSendCompleted(size_t bytesTransferred) = 0;
        virtual void OnSendFailed() = 0;
        virtual void OnRecvCompleted(size_t bytesTransferred) = 0;

        virtual void OnPacketValidationSucceeded() = 0;
        virtual void OnPacketValidationFailed() = 0;

        virtual void TrackRequest(
            uint32_t sequence,
            int32_t responseType,
            std::chrono::milliseconds timeout) = 0;
        virtual void ResolveRequest(
            uint32_t sequence,
            int32_t receivedType) = 0;

        [[nodiscard]] virtual ClientMetricsSnapshot TakeSnapshot() const = 0;
    };
} // namespace highp::metrics
