#pragma once

#include "metrics/server/ServerMetricsSnapshot.h"
#include <chrono>

namespace highp::metrics {
    class IServerMetrics {
    public:
        virtual ~IServerMetrics() = default;

        virtual void OnAcceptPosted() = 0;
        virtual void OnAcceptPostFailed() = 0;
        virtual void OnAcceptCompleted() = 0;
        virtual void OnAcceptCompletionFailed() = 0;

        virtual void OnClientConnected() = 0;
        virtual void OnClientDisconnected() = 0;

        virtual void OnRecvPosted() = 0;
        virtual void OnRecvPostFailed() = 0;
        virtual void OnRecvCompleted(size_t bytesTransferred, bool countPayload) = 0;

        virtual void OnSendPosted() = 0;
        virtual void OnSendPostFailed() = 0;
        virtual void OnSendCompleted(size_t bytesTransferred, bool countPayload) = 0;

        virtual void OnWorkerDispatchBegin() = 0;
        virtual void OnWorkerDispatchEnd() = 0;

        virtual void OnPacketValidationSucceeded() = 0;
        virtual void OnPacketValidationFailed() = 0;

        virtual void OnQueuePushed(size_t count = 1) = 0;
        virtual void OnQueueDrained(size_t count = 1) = 0;
        virtual void ObserveQueueWait(std::chrono::nanoseconds duration) = 0;

        virtual void ObserveDispatchProcess(std::chrono::nanoseconds duration) = 0;
        virtual void ObserveTickDuration(std::chrono::nanoseconds duration) = 0;
        virtual void ObserveTickLag(std::chrono::nanoseconds duration) = 0;

        [[nodiscard]] virtual ServerMetricsSnapshot TakeSnapshot() = 0;
    };
} // namespace highp::metrics
