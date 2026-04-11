#pragma once

#include "metrics/IClientMetrics.h"

namespace highp::metrics {
    class NoopClientMetrics final : public IClientMetrics {
    public:
        void ObserveConnectLatency(std::chrono::nanoseconds, bool) override {}
        void OnConnected() override {}
        void OnDisconnected(ClientDisconnectReason) override {}

        void OnSendCompleted(size_t) override {}
        void OnSendFailed() override {}
        void OnRecvCompleted(size_t) override {}

        void OnPacketValidationSucceeded() override {}
        void OnPacketValidationFailed() override {}

        void TrackRequest(
            uint32_t,
            int32_t,
            std::chrono::milliseconds) override {}
        void ResolveRequest(uint32_t, int32_t) override {}

        [[nodiscard]] ClientMetricsSnapshot TakeSnapshot() const override {
            return {};
        }
    };
} // namespace highp::metrics
