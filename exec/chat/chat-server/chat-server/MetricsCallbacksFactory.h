#pragma once

#include <memory>

#include "Server.h"

namespace highp::metrics {
    class IServerMetrics;
    class ServerMetricsWriter;
}

struct MetricsCallbacksBundle {
    GameLoop::EventCallbacks gameLoop;
    Server::EventCallbacks server;
};

[[nodiscard]] MetricsCallbacksBundle MakeMetricsCallbacksBundle(
    std::shared_ptr<highp::metrics::IServerMetrics> metrics,
    std::shared_ptr<highp::metrics::ServerMetricsWriter> metricsWriter);
