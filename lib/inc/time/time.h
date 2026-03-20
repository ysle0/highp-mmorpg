#pragma once

#include <chrono>

namespace highp::time {
    class Time {
    public:
        static decltype(auto) Now() {
            return std::chrono::steady_clock::now();
        }

        static decltype(auto) NowInMs() {
            const auto epoch = Now().time_since_epoch();
            return std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        }

        static decltype(auto) NowInSec() {
            const auto epoch = Now().time_since_epoch();
            return std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
        }

        static decltype(auto) NowInNs() {
            const auto epoch = Now().time_since_epoch();
            return static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count());
        }

        static decltype(auto) NowInNs64() {
            const auto epoch = Now().time_since_epoch();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();
        }
    };
}
