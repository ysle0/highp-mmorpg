#include "MetricsCallbacksFactory.h"

#include <iostream>
#include <utility>

#include <metrics/server/ServerMetricsCallbacks.h>
#include <metrics/server/ServerMetricsWriter.h>

namespace {
    [[nodiscard]] highp::net::PacketDispatcherCallbacks MakePacketDispatcherCallbacks(
        const highp::metrics::DispatcherMetricsCallbacks& callbacks
    ) {
        return highp::net::PacketDispatcherCallbacks{
            .onPacketValidationSucceeded = callbacks.onPacketValidationSucceeded,
            .onPacketValidationFailed = callbacks.onPacketValidationFailed,
            .onQueuePushed = callbacks.onQueuePushed,
            .onQueueDrained = callbacks.onQueueDrained,
            .onQueueWait = callbacks.observeQueueWait,
            .onDispatchProcess = callbacks.observeDispatchProcess,
        };
    }

    [[nodiscard]] highp::net::ServerLifeCycle::EventCallbacks MakeLifeCycleCallbacks(
        const highp::metrics::ServerMetricsCallbacks& callbacks
    ) {
        return highp::net::ServerLifeCycle::EventCallbacks{
            .client = highp::net::Client::EventCallbacks{
                .onConnected = callbacks.connection.onClientConnected,
                .onDisconnected = callbacks.connection.onClientDisconnected,
                .onRecvPosted = callbacks.connection.onRecvPosted,
                .onRecvPostFailed = callbacks.connection.onRecvPostFailed,
                .onSendPosted = callbacks.connection.onSendPosted,
                .onSendPostFailed = callbacks.connection.onSendPostFailed,
            },
            .acceptor = highp::net::internal::IocpAcceptor::EventCallbacks{
                .onAcceptPosted = callbacks.acceptor.onAcceptPosted,
                .onAcceptPostFailed = callbacks.acceptor.onAcceptPostFailed,
                .onAcceptCompleted = callbacks.acceptor.onAcceptCompleted,
                .onAcceptCompletionFailed = callbacks.acceptor.onAcceptCompletionFailed,
            },
            .iocp = highp::net::internal::IocpIoMultiplexer::EventCallbacks{
                .onWorkerDispatchBegin = callbacks.worker.onWorkerDispatchBegin,
                .onWorkerDispatchEnd = callbacks.worker.onWorkerDispatchEnd,
            },
            .onRecvCompleted = callbacks.connection.onRecvCompleted,
            .onSendCompleted = callbacks.connection.onSendCompleted,
        };
    }
} // namespace

MetricsCallbacksBundle MakeMetricsCallbacksBundle(
    std::shared_ptr<highp::metrics::IServerMetrics> metrics,
    std::shared_ptr<highp::metrics::ServerMetricsWriter> metricsWriter
) {
    const highp::metrics::ServerMetricsCallbacks metricsCallbacks =
        highp::metrics::BindServerMetricsCallbacks(std::move(metrics));

    MetricsCallbacksBundle bundle{};
    bundle.gameLoop = GameLoop::EventCallbacks{
        .dispatcher = MakePacketDispatcherCallbacks(metricsCallbacks.dispatcher),
        .onTickDuration = metricsCallbacks.logic.observeTickDuration,
        .onTickLag = metricsCallbacks.logic.observeTickLag,
        .onLogicThreadStarted = [metricsWriter](DWORD threadId) {
            if (metricsWriter) {
                metricsWriter->SetLogicThreadId(threadId);
            }
        },
    };
    bundle.server = Server::EventCallbacks{
        .lifecycle = MakeLifeCycleCallbacks(metricsCallbacks),
        .onStarted = [metricsWriter]() {
            if (metricsWriter && !metricsWriter->Start()) {
                std::cerr << "Failed to start server metrics writer: "
                    << metricsWriter->LastError() << '\n';
            }
        },
        .onStopping = [metricsWriter]() {
            if (metricsWriter) {
                metricsWriter->Stop();
            }
        },
    };

    return bundle;
}
