#pragma once

#include "metrics/IServerMetrics.h"
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>

namespace highp::metrics {
    struct AcceptorMetricsCallbacks {
        std::function<void()> onAcceptPosted;
        std::function<void()> onAcceptPostFailed;
        std::function<void()> onAcceptCompleted;
        std::function<void()> onAcceptCompletionFailed;

        void AcceptPosted() const;
        void AcceptPostFailed() const;
        void AcceptCompleted() const;
        void AcceptCompletionFailed() const;
    };

    struct ConnectionMetricsCallbacks {
        std::function<void()> onClientConnected;
        std::function<void()> onClientDisconnected;
        std::function<void()> onRecvPosted;
        std::function<void()> onRecvPostFailed;
        std::function<void(std::size_t, bool)> onRecvCompleted;
        std::function<void()> onSendPosted;
        std::function<void()> onSendPostFailed;
        std::function<void(std::size_t, bool)> onSendCompleted;

        void ClientConnected() const;
        void ClientDisconnected() const;
        void RecvPosted() const;
        void RecvPostFailed() const;
        void RecvCompleted(std::size_t bytesTransferred, bool countPayload) const;
        void SendPosted() const;
        void SendPostFailed() const;
        void SendCompleted(std::size_t bytesTransferred, bool countPayload) const;
    };

    struct WorkerMetricsCallbacks {
        std::function<void()> onWorkerDispatchBegin;
        std::function<void()> onWorkerDispatchEnd;

        void WorkerDispatchBegin() const;
        void WorkerDispatchEnd() const;
    };

    struct DispatcherMetricsCallbacks {
        std::function<void()> onPacketValidationSucceeded;
        std::function<void()> onPacketValidationFailed;
        std::function<void(std::size_t)> onQueuePushed;
        std::function<void(std::size_t)> onQueueDrained;
        std::function<void(std::chrono::nanoseconds)> observeQueueWait;
        std::function<void(std::chrono::nanoseconds)> observeDispatchProcess;

        void PacketValidationSucceeded() const;
        void PacketValidationFailed() const;
        void QueuePushed(std::size_t count = 1) const;
        void QueueDrained(std::size_t count = 1) const;
        void QueueWait(std::chrono::nanoseconds duration) const;
        void DispatchProcess(std::chrono::nanoseconds duration) const;
    };

    struct LogicMetricsCallbacks {
        std::function<void(std::chrono::nanoseconds)> observeTickDuration;
        std::function<void(std::chrono::nanoseconds)> observeTickLag;

        void TickDuration(std::chrono::nanoseconds duration) const;
        void TickLag(std::chrono::nanoseconds duration) const;
    };

    struct ServerMetricsCallbacks {
        AcceptorMetricsCallbacks acceptor;
        ConnectionMetricsCallbacks connection;
        WorkerMetricsCallbacks worker;
        DispatcherMetricsCallbacks dispatcher;
        LogicMetricsCallbacks logic;
    };

    [[nodiscard]] AcceptorMetricsCallbacks BindAcceptorMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics);
    [[nodiscard]] ConnectionMetricsCallbacks BindConnectionMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics);
    [[nodiscard]] WorkerMetricsCallbacks BindWorkerMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics);
    [[nodiscard]] DispatcherMetricsCallbacks BindDispatcherMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics);
    [[nodiscard]] LogicMetricsCallbacks BindLogicMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics);
    [[nodiscard]] ServerMetricsCallbacks BindServerMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics);

    [[nodiscard]] AcceptorMetricsCallbacks MakeNoopAcceptorMetricsCallbacks() noexcept;
    [[nodiscard]] ConnectionMetricsCallbacks MakeNoopConnectionMetricsCallbacks() noexcept;
    [[nodiscard]] WorkerMetricsCallbacks MakeNoopWorkerMetricsCallbacks() noexcept;
    [[nodiscard]] DispatcherMetricsCallbacks MakeNoopDispatcherMetricsCallbacks() noexcept;
    [[nodiscard]] LogicMetricsCallbacks MakeNoopLogicMetricsCallbacks() noexcept;
    [[nodiscard]] ServerMetricsCallbacks MakeNoopServerMetricsCallbacks() noexcept;
} // namespace highp::metrics
