#pragma once

#include <cstdint>
#include <chrono>

namespace highp::metrics {
    struct TimingWindow {
        uint64_t count = 0;
        uint64_t totalNanoseconds = 0;
        uint64_t maxNanoseconds = 0;

        [[nodiscard]] double AverageMilliseconds() const noexcept {
            if (count == 0) {
                return 0.0;
            }

            const auto totalMs = static_cast<double>(totalNanoseconds) / 1'000'000.0;
            return totalMs / static_cast<double>(count);
        }

        [[nodiscard]] double MaxMilliseconds() const noexcept {
            return static_cast<double>(maxNanoseconds) / 1'000'000.0;
        }
    };

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
