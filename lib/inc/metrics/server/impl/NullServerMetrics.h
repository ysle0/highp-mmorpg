#pragma once

#include "metrics/server/IServerMetrics.h"

namespace highp::metrics {
    class NullServerMetrics final : public IServerMetrics {
    public:
        void OnAcceptPosted() override {
        }

        void OnAcceptPostFailed() override {
        }

        void OnAcceptCompleted() override {
        }

        void OnAcceptCompletionFailed() override {
        }

        void OnClientConnected() override {
        }

        void OnClientDisconnected() override {
        }

        void OnRecvPosted() override {
        }

        void OnRecvPostFailed() override {
        }

        void OnRecvCompleted(size_t, bool) override {
        }

        void OnSendPosted() override {
        }

        void OnSendPostFailed() override {
        }

        void OnSendCompleted(size_t, bool) override {
        }

        void OnWorkerDispatchBegin() override {
        }

        void OnWorkerDispatchEnd() override {
        }

        void OnPacketValidationSucceeded() override {
        }

        void OnPacketValidationFailed() override {
        }

        void OnQueuePushed(size_t = 1) override {
        }

        void OnQueueDrained(size_t = 1) override {
        }

        void ObserveQueueWait(std::chrono::nanoseconds) override {
        }

        void ObserveDispatchProcess(std::chrono::nanoseconds) override {
        }

        void ObserveTickDuration(std::chrono::nanoseconds) override {
        }

        void ObserveTickLag(std::chrono::nanoseconds) override {
        }

        [[nodiscard]] ServerMetricsSnapshot TakeSnapshot() override {
            return {};
        }
    };
} // namespace highp::metrics
