#pragma once

#include <chrono>
#include <cstdint>

namespace highp::metrics {
    struct SnapshotRecord {
        std::chrono::system_clock::time_point capturedAt{};
        std::chrono::steady_clock::time_point capturedAtSteady{};
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
        double queueWaitMsAvg = 0.0;
        double queueWaitMsMax = 0.0;
        double dispatchProcessMsAvg = 0.0;
        double dispatchProcessMsMax = 0.0;
        double tickDurationMsAvg = 0.0;
        double tickDurationMsMax = 0.0;
        double tickLagMsAvg = 0.0;
        double tickLagMsMax = 0.0;
        double processCpuPercent = 0.0;
        double logicThreadCpuPercent = 0.0;
        uint32_t threadCount = 0;
    };
} // namespace highp::metrics
