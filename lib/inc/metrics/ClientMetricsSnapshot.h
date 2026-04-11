#pragma once

#include "metrics/TimingWindow.h"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace highp::metrics {
    enum class ClientDisconnectReason : uint8_t {
        None = 0,
        Timeout,
        ServerClose,
        RecvFail,
        LocalClose,
    };

    [[nodiscard]] inline std::string_view ToString(ClientDisconnectReason reason) noexcept {
        switch (reason) {
        case ClientDisconnectReason::Timeout:
            return "timeout";
        case ClientDisconnectReason::ServerClose:
            return "server close";
        case ClientDisconnectReason::RecvFail:
            return "recv fail";
        case ClientDisconnectReason::LocalClose:
            return "local close";
        case ClientDisconnectReason::None:
        default:
            return "none";
        }
    }

    struct ClientMetricsSnapshot {
        using Clock = std::chrono::system_clock;

        Clock::time_point capturedAt{};

        uint64_t connectSuccessTotal = 0;
        uint64_t connectFailureTotal = 0;
        uint64_t disconnectTotal = 0;
        uint64_t sendPacketsTotal = 0;
        uint64_t recvPacketsTotal = 0;
        uint64_t sendBytesTotal = 0;
        uint64_t recvBytesTotal = 0;
        uint64_t sendFailTotal = 0;
        uint64_t responseTimeoutTotal = 0;
        uint64_t packetValidationTotal = 0;
        uint64_t packetValidationFailureTotal = 0;

        uint64_t disconnectTimeoutTotal = 0;
        uint64_t disconnectServerCloseTotal = 0;
        uint64_t disconnectRecvFailTotal = 0;
        uint64_t disconnectLocalCloseTotal = 0;

        int64_t connected = 0;
        uint32_t pendingRequestCount = 0;

        TimingWindow connectLatency;
        TimingWindow requestRtt;

        double lastConnectLatencyMs = 0.0;
        uint64_t currentSessionUptimeMs = 0;
        uint64_t lastSessionUptimeMs = 0;
        uint64_t maxSessionUptimeMs = 0;
        uint64_t timeSinceLastRecvMs = 0;
        ClientDisconnectReason lastDisconnectReason = ClientDisconnectReason::None;

        double processCpuPercent = 0.0;
        uint32_t threadCount = 0;
    };
} // namespace highp::metrics
