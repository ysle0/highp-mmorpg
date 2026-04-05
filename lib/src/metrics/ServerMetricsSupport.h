#pragma once

#include "metrics/ServerMetricsSnapshot.h"

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace highp::metrics::internal {
    [[nodiscard]] std::string Trim(std::string value);
    [[nodiscard]] std::string ToLower(std::string value);
    [[nodiscard]] bool ParseBool(std::string value, bool fallback);
    [[nodiscard]] std::optional<std::string> ReadEnv(const char* name);
    [[nodiscard]] std::filesystem::path ReadEnvPath(const char* name);
    [[nodiscard]] std::string SanitizePathComponent(std::string value);
    [[nodiscard]] uint64_t FileTimeToUint64(const FILETIME& value) noexcept;
    [[nodiscard]] std::string FormatUtcTime(const std::chrono::system_clock::time_point& tp);
    [[nodiscard]] std::string FormatCompactTimestamp(const std::chrono::system_clock::time_point& tp);
    [[nodiscard]] uint32_t CountProcessThreads();
    [[nodiscard]] uint32_t GetProcessorCount();
    [[nodiscard]] std::string JsonEscape(std::string_view value);
    [[nodiscard]] std::string FormatNumber(double value, int precision = 3);
    [[nodiscard]] double TimingAverageMs(const TimingWindow& window);
    [[nodiscard]] double TimingMaxMs(const TimingWindow& window);
} // namespace highp::metrics::internal
