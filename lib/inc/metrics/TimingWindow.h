#pragma once

#include <cstdint>

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
} // namespace highp::metrics
