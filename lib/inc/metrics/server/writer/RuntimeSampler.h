#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <Windows.h>

namespace highp::metrics {
    struct RuntimeSample {
        double processCpuPercent = 0.0;
        double logicThreadCpuPercent = 0.0;
        uint32_t threadCount = 0;
    };

    class RuntimeSampler final {
    public:
        RuntimeSample Sample();
        void SetLogicThreadId(DWORD threadId) noexcept;
        void Reset() noexcept;

    private:
        std::atomic<DWORD> _logicThreadId{0};
        std::chrono::steady_clock::time_point _lastSampleAt{};
        uint64_t _lastProcessKernel100ns = 0;
        uint64_t _lastProcessUser100ns = 0;
        uint64_t _lastLogicThreadKernel100ns = 0;
        uint64_t _lastLogicThreadUser100ns = 0;
        DWORD _lastLogicThreadIdSampled = 0;
        bool _hasBaseline = false;
    };
} // namespace highp::metrics
