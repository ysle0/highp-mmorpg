#include "pch.h"

#include "metrics/server/ServerMetricsCallbacks.h"

namespace highp::metrics { namespace {
        template <typename Fn, typename F>
        Fn BindVoidMetric(std::shared_ptr<IServerMetrics> metrics, F method) {
            if (!metrics) {
                return Fn{};
            }

            return Fn([metrics = std::move(metrics), method]() mutable {
                ((*metrics).*method)();
            });
        }

        template <typename Fn, typename F>
        Fn BindSizeTMetric(std::shared_ptr<IServerMetrics> metrics, F method) {
            if (!metrics) {
                return Fn{};
            }

            return Fn([metrics = std::move(metrics), method](std::size_t value) mutable {
                ((*metrics).*method)(value);
            });
        }

        template <typename Fn, typename F>
        Fn BindSizeTBoolMetric(std::shared_ptr<IServerMetrics> metrics, F method) {
            if (!metrics) {
                return Fn{};
            }

            return Fn([metrics = std::move(metrics), method](std::size_t bytesTransferred, bool countPayload) mutable {
                ((*metrics).*method)(bytesTransferred, countPayload);
            });
        }

        template <typename Fn, typename F>
        Fn BindDurationMetric(std::shared_ptr<IServerMetrics> metrics, F method) {
            if (!metrics) {
                return Fn{};
            }

            return Fn([metrics = std::move(metrics), method](std::chrono::nanoseconds duration) mutable {
                ((*metrics).*method)(duration);
            });
        }
    } // namespace

    void AcceptorMetricsCallbacks::AcceptPosted() const {
        if (onAcceptPosted) {
            onAcceptPosted();
        }
    }

    void AcceptorMetricsCallbacks::AcceptPostFailed() const {
        if (onAcceptPostFailed) {
            onAcceptPostFailed();
        }
    }

    void AcceptorMetricsCallbacks::AcceptCompleted() const {
        if (onAcceptCompleted) {
            onAcceptCompleted();
        }
    }

    void AcceptorMetricsCallbacks::AcceptCompletionFailed() const {
        if (onAcceptCompletionFailed) {
            onAcceptCompletionFailed();
        }
    }

    void ConnectionMetricsCallbacks::ClientConnected() const {
        if (onClientConnected) {
            onClientConnected();
        }
    }

    void ConnectionMetricsCallbacks::ClientDisconnected() const {
        if (onClientDisconnected) {
            onClientDisconnected();
        }
    }

    void ConnectionMetricsCallbacks::RecvPosted() const {
        if (onRecvPosted) {
            onRecvPosted();
        }
    }

    void ConnectionMetricsCallbacks::RecvPostFailed() const {
        if (onRecvPostFailed) {
            onRecvPostFailed();
        }
    }

    void ConnectionMetricsCallbacks::RecvCompleted(std::size_t bytesTransferred, bool countPayload) const {
        if (onRecvCompleted) {
            onRecvCompleted(bytesTransferred, countPayload);
        }
    }

    void ConnectionMetricsCallbacks::SendPosted() const {
        if (onSendPosted) {
            onSendPosted();
        }
    }

    void ConnectionMetricsCallbacks::SendPostFailed() const {
        if (onSendPostFailed) {
            onSendPostFailed();
        }
    }

    void ConnectionMetricsCallbacks::SendCompleted(std::size_t bytesTransferred, bool countPayload) const {
        if (onSendCompleted) {
            onSendCompleted(bytesTransferred, countPayload);
        }
    }

    void WorkerMetricsCallbacks::WorkerDispatchBegin() const {
        if (onWorkerDispatchBegin) {
            onWorkerDispatchBegin();
        }
    }

    void WorkerMetricsCallbacks::WorkerDispatchEnd() const {
        if (onWorkerDispatchEnd) {
            onWorkerDispatchEnd();
        }
    }

    void DispatcherMetricsCallbacks::PacketValidationSucceeded() const {
        if (onPacketValidationSucceeded) {
            onPacketValidationSucceeded();
        }
    }

    void DispatcherMetricsCallbacks::PacketValidationFailed() const {
        if (onPacketValidationFailed) {
            onPacketValidationFailed();
        }
    }

    void DispatcherMetricsCallbacks::QueuePushed(std::size_t count) const {
        if (onQueuePushed) {
            onQueuePushed(count);
        }
    }

    void DispatcherMetricsCallbacks::QueueDrained(std::size_t count) const {
        if (onQueueDrained) {
            onQueueDrained(count);
        }
    }

    void DispatcherMetricsCallbacks::QueueWait(std::chrono::nanoseconds duration) const {
        if (observeQueueWait) {
            observeQueueWait(duration);
        }
    }

    void DispatcherMetricsCallbacks::DispatchProcess(std::chrono::nanoseconds duration) const {
        if (observeDispatchProcess) {
            observeDispatchProcess(duration);
        }
    }

    void LogicMetricsCallbacks::TickDuration(std::chrono::nanoseconds duration) const {
        if (observeTickDuration) {
            observeTickDuration(duration);
        }
    }

    void LogicMetricsCallbacks::TickLag(std::chrono::nanoseconds duration) const {
        if (observeTickLag) {
            observeTickLag(duration);
        }
    }

    AcceptorMetricsCallbacks BindAcceptorMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics) {
        if (!metrics) {
            return MakeNoopAcceptorMetricsCallbacks();
        }
        AcceptorMetricsCallbacks callbacks{};
        callbacks.onAcceptPosted = BindVoidMetric<std::function<void()>>(metrics, &IServerMetrics::OnAcceptPosted);
        callbacks.onAcceptPostFailed = BindVoidMetric<std::function<void()>>(
            metrics, &IServerMetrics::OnAcceptPostFailed);
        callbacks.onAcceptCompleted = BindVoidMetric<std::function<void
            ()>>(metrics, &IServerMetrics::OnAcceptCompleted);
        callbacks.onAcceptCompletionFailed = BindVoidMetric<std::function<void()>>(
            metrics, &IServerMetrics::OnAcceptCompletionFailed);
        return callbacks;
    }

    ConnectionMetricsCallbacks BindConnectionMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics) {
        if (!metrics) {
            return MakeNoopConnectionMetricsCallbacks();
        }
        ConnectionMetricsCallbacks callbacks{};
        callbacks.onClientConnected = BindVoidMetric<std::function<void
            ()>>(metrics, &IServerMetrics::OnClientConnected);
        callbacks.onClientDisconnected = BindVoidMetric<std::function<void()>>(
            metrics, &IServerMetrics::OnClientDisconnected);
        callbacks.onRecvPosted = BindVoidMetric<std::function<void()>>(metrics, &IServerMetrics::OnRecvPosted);
        callbacks.onRecvPostFailed = BindVoidMetric<std::function<void()>>(metrics, &IServerMetrics::OnRecvPostFailed);
        callbacks.onRecvCompleted = BindSizeTBoolMetric<std::function<void(std::size_t, bool)>>(
            metrics, &IServerMetrics::OnRecvCompleted);
        callbacks.onSendPosted = BindVoidMetric<std::function<void()>>(metrics, &IServerMetrics::OnSendPosted);
        callbacks.onSendPostFailed = BindVoidMetric<std::function<void()>>(metrics, &IServerMetrics::OnSendPostFailed);
        callbacks.onSendCompleted = BindSizeTBoolMetric<std::function<void(std::size_t, bool)>>(
            metrics, &IServerMetrics::OnSendCompleted);
        return callbacks;
    }

    WorkerMetricsCallbacks BindWorkerMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics) {
        if (!metrics) {
            return MakeNoopWorkerMetricsCallbacks();
        }
        WorkerMetricsCallbacks callbacks{};
        callbacks.onWorkerDispatchBegin = BindVoidMetric<std::function<void()>>(
            metrics, &IServerMetrics::OnWorkerDispatchBegin);
        callbacks.onWorkerDispatchEnd = BindVoidMetric<std::function<void()>>(
            metrics, &IServerMetrics::OnWorkerDispatchEnd);
        return callbacks;
    }

    DispatcherMetricsCallbacks BindDispatcherMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics) {
        if (!metrics) {
            return MakeNoopDispatcherMetricsCallbacks();
        }
        DispatcherMetricsCallbacks callbacks{};
        callbacks.onPacketValidationSucceeded = BindVoidMetric<std::function<void()>>(
            metrics, &IServerMetrics::OnPacketValidationSucceeded);
        callbacks.onPacketValidationFailed = BindVoidMetric<std::function<void()>>(
            metrics, &IServerMetrics::OnPacketValidationFailed);
        callbacks.onQueuePushed = BindSizeTMetric<std::function<void(std::size_t)>>(
            metrics, &IServerMetrics::OnQueuePushed);
        callbacks.onQueueDrained = BindSizeTMetric<std::function<void(std::size_t)>>(
            metrics, &IServerMetrics::OnQueueDrained);
        callbacks.observeQueueWait = BindDurationMetric<std::function<void(std::chrono::nanoseconds)>>(
            metrics, &IServerMetrics::ObserveQueueWait);
        callbacks.observeDispatchProcess = BindDurationMetric<std::function<void(std::chrono::nanoseconds)>>(
            metrics, &IServerMetrics::ObserveDispatchProcess);
        return callbacks;
    }

    LogicMetricsCallbacks BindLogicMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics) {
        if (!metrics) {
            return MakeNoopLogicMetricsCallbacks();
        }
        LogicMetricsCallbacks callbacks{};
        callbacks.observeTickDuration = BindDurationMetric<std::function<void(std::chrono::nanoseconds)>>(
            metrics, &IServerMetrics::ObserveTickDuration);
        callbacks.observeTickLag = BindDurationMetric<std::function<void(std::chrono::nanoseconds)>>(
            metrics, &IServerMetrics::ObserveTickLag);
        return callbacks;
    }

    ServerMetricsCallbacks BindServerMetricsCallbacks(std::shared_ptr<IServerMetrics> metrics) {
        ServerMetricsCallbacks callbacks{};
        callbacks.acceptor = BindAcceptorMetricsCallbacks(metrics);
        callbacks.connection = BindConnectionMetricsCallbacks(metrics);
        callbacks.worker = BindWorkerMetricsCallbacks(metrics);
        callbacks.dispatcher = BindDispatcherMetricsCallbacks(metrics);
        callbacks.logic = BindLogicMetricsCallbacks(metrics);
        return callbacks;
    }

    AcceptorMetricsCallbacks MakeNoopAcceptorMetricsCallbacks() noexcept {
        AcceptorMetricsCallbacks callbacks{};
        callbacks.onAcceptPosted = [] {
        };
        callbacks.onAcceptPostFailed = [] {
        };
        callbacks.onAcceptCompleted = [] {
        };
        callbacks.onAcceptCompletionFailed = [] {
        };
        return callbacks;
    }

    ConnectionMetricsCallbacks MakeNoopConnectionMetricsCallbacks() noexcept {
        ConnectionMetricsCallbacks callbacks{};
        callbacks.onClientConnected = [] {
        };
        callbacks.onClientDisconnected = [] {
        };
        callbacks.onRecvPosted = [] {
        };
        callbacks.onRecvPostFailed = [] {
        };
        callbacks.onRecvCompleted = [](std::size_t, bool) {
        };
        callbacks.onSendPosted = [] {
        };
        callbacks.onSendPostFailed = [] {
        };
        callbacks.onSendCompleted = [](std::size_t, bool) {
        };
        return callbacks;
    }

    WorkerMetricsCallbacks MakeNoopWorkerMetricsCallbacks() noexcept {
        WorkerMetricsCallbacks callbacks{};
        callbacks.onWorkerDispatchBegin = [] {
        };
        callbacks.onWorkerDispatchEnd = [] {
        };
        return callbacks;
    }

    DispatcherMetricsCallbacks MakeNoopDispatcherMetricsCallbacks() noexcept {
        DispatcherMetricsCallbacks callbacks{};
        callbacks.onPacketValidationSucceeded = [] {
        };
        callbacks.onPacketValidationFailed = [] {
        };
        callbacks.onQueuePushed = [](std::size_t) {
        };
        callbacks.onQueueDrained = [](std::size_t) {
        };
        callbacks.observeQueueWait = [](std::chrono::nanoseconds) {
        };
        callbacks.observeDispatchProcess = [](std::chrono::nanoseconds) {
        };
        return callbacks;
    }

    LogicMetricsCallbacks MakeNoopLogicMetricsCallbacks() noexcept {
        LogicMetricsCallbacks callbacks{};
        callbacks.observeTickDuration = [](std::chrono::nanoseconds) {
        };
        callbacks.observeTickLag = [](std::chrono::nanoseconds) {
        };
        return callbacks;
    }

    ServerMetricsCallbacks MakeNoopServerMetricsCallbacks() noexcept {
        ServerMetricsCallbacks callbacks{};
        callbacks.acceptor = MakeNoopAcceptorMetricsCallbacks();
        callbacks.connection = MakeNoopConnectionMetricsCallbacks();
        callbacks.worker = MakeNoopWorkerMetricsCallbacks();
        callbacks.dispatcher = MakeNoopDispatcherMetricsCallbacks();
        callbacks.logic = MakeNoopLogicMetricsCallbacks();
        return callbacks;
    }
} // namespace highp::metrics
