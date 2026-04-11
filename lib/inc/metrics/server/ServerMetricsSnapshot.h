#pragma once

#include "metrics/TimingWindow.h"

#include <chrono>
#include <cstdint>

namespace highp::metrics {
    struct ServerMetricsSnapshot {
        using Clock = std::chrono::system_clock;

        Clock::time_point capturedAt{};

        uint64_t acceptTotal = 0;
        uint64_t disconnectTotal = 0;
        uint64_t recvPacketsTotal = 0;
        uint64_t sendPacketsTotal = 0;
        uint64_t recvBytesTotal = 0;
        uint64_t sendBytesTotal = 0;
        uint64_t sendFailTotal = 0;
        uint64_t packetValidationTotal = 0;
        uint64_t packetValidationFailureTotal = 0;

        int64_t connectedClients = 0;
        int64_t pendingAcceptCount = 0;
        int64_t pendingRecvCount = 0;
        int64_t pendingSendCount = 0;
        int64_t dispatcherQueueLength = 0;
        int64_t runnableWorkerThreadCount = 0;

        TimingWindow queueWait;
        TimingWindow dispatchProcess;
        TimingWindow tickDuration;
        TimingWindow tickLag;

        double processCpuPercent = 0.0;
        double logicThreadCpuPercent = 0.0;
        uint32_t threadCount = 0;
    };
} // namespace highp::metrics
