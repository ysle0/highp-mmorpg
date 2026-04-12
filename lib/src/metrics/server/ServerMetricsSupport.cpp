#include "pch.h"

#include "metrics/server/ServerMetricsSupport.h"

#include "scope/DeferContext.hpp"

#include <TlHelp32.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>

namespace highp::metrics::internal {
    std::string Trim(std::string value) {
        const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };

        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }

        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }

        return value;
    }

    std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    bool ParseBool(std::string value, bool fallback) {
        value = ToLower(Trim(std::move(value)));
        if (value.empty()) {
            return fallback;
        }

        if (value == "1" || value == "true" || value == "yes" || value == "on") {
            return true;
        }

        if (value == "0" || value == "false" || value == "no" || value == "off") {
            return false;
        }

        return fallback;
    }

    uint64_t ParseUnsigned(std::string value, uint64_t fallback) {
        value = Trim(std::move(value));
        if (value.empty()) {
            return fallback;
        }

        uint64_t parsed = 0;
        const auto* begin = value.data();
        const auto* end = value.data() + value.size();
        const auto [ptr, ec] = std::from_chars(begin, end, parsed);
        if (ec != std::errc{} || ptr != end) {
            return fallback;
        }

        return parsed;
    }

    std::optional<std::string> ReadEnv(const char* name) {
        char* rawValue = nullptr;
        size_t rawValueLength = 0;
        if (_dupenv_s(&rawValue, &rawValueLength, name) != 0 || rawValue == nullptr) {
            return std::nullopt;
        }

        std::unique_ptr<char, decltype(&std::free)> value(rawValue, &std::free);
        std::string result(value.get());
        result = Trim(std::move(result));
        if (!result.empty()) {
            return result;
        }

        return std::nullopt;
    }

    std::filesystem::path ReadEnvPath(const char* name) {
        if (auto value = ReadEnv(name)) {
            return std::filesystem::path{*value};
        }

        return {};
    }

    std::string SanitizePathComponent(std::string value) {
        for (char& ch : value) {
            const unsigned char byte = static_cast<unsigned char>(ch);
            const bool invalid = byte < 32 ||
                ch == '<' ||
                ch == '>' ||
                ch == ':' ||
                ch == '"' ||
                ch == '/' ||
                ch == '\\' ||
                ch == '|' ||
                ch == '?' ||
                ch == '*' ||
                std::isspace(byte) != 0;
            if (invalid) {
                ch = '_';
            }
        }

        return value;
    }

    uint64_t FileTimeToUint64(const FILETIME& value) noexcept {
        ULARGE_INTEGER converted{};
        converted.LowPart = value.dwLowDateTime;
        converted.HighPart = value.dwHighDateTime;
        return converted.QuadPart;
    }

    std::string FormatUtcTime(const std::chrono::system_clock::time_point& tp) {
        const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        const auto fractional = std::chrono::duration_cast<std::chrono::milliseconds>(tp - seconds);
        const std::time_t rawTime = std::chrono::system_clock::to_time_t(seconds);

        std::tm utc{};
        gmtime_s(&utc, &rawTime);

        std::ostringstream oss;
        oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setw(3) << std::setfill('0') << fractional.count() << 'Z';
        return oss.str();
    }

    std::string FormatCompactTimestamp(const std::chrono::system_clock::time_point& tp) {
        const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        const auto fractional = std::chrono::duration_cast<std::chrono::milliseconds>(tp - seconds);
        const std::time_t rawTime = std::chrono::system_clock::to_time_t(seconds);

        std::tm utc{};
        gmtime_s(&utc, &rawTime);

        std::ostringstream oss;
        oss << std::put_time(&utc, "%Y%m%d-%H%M%S");
        if (fractional.count() > 0) {
            oss << '-' << std::setw(3) << std::setfill('0') << fractional.count();
        }

        return oss.str();
    }

    uint32_t CountProcessThreads() {
        const DWORD processId = GetCurrentProcessId();
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return 0;
        }

        highp::scope::DeferContext<HANDLE> snapshotCleanup(&snapshot, [](HANDLE* handle) {
            if (*handle != nullptr && *handle != INVALID_HANDLE_VALUE) {
                CloseHandle(*handle);
            }
        });

        uint32_t count = 0;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        if (Thread32First(snapshot, &entry)) {
            do {
                if (entry.th32OwnerProcessID == processId) {
                    ++count;
                }
            }
            while (Thread32Next(snapshot, &entry));
        }

        return count;
    }

    uint32_t GetProcessorCount() {
        DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (count == 0) {
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            count = info.dwNumberOfProcessors;
        }

        return count == 0 ? 1u : static_cast<uint32_t>(count);
    }

    std::string JsonEscape(std::string_view value) {
        std::ostringstream oss;
        for (const char ch : value) {
            switch (ch) {
            case '\\': oss << "\\\\";
                break;
            case '"': oss << "\\\"";
                break;
            case '\b': oss << "\\b";
                break;
            case '\f': oss << "\\f";
                break;
            case '\n': oss << "\\n";
                break;
            case '\r': oss << "\\r";
                break;
            case '\t': oss << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    oss << "\\u"
                        << std::hex
                        << std::setw(4)
                        << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch))
                        << std::dec;
                }
                else {
                    oss << ch;
                }
                break;
            }
        }

        return oss.str();
    }

    std::string FormatNumber(double value, int precision) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        std::string result = oss.str();

        if (auto dot = result.find('.'); dot != std::string::npos) {
            while (!result.empty() && result.back() == '0') {
                result.pop_back();
            }

            if (!result.empty() && result.back() == '.') {
                result.pop_back();
            }
        }

        if (result == "-0") {
            result = "0";
        }

        return result;
    }

    double TimingAverageMs(const TimingWindow& window) {
        return window.AverageMilliseconds();
    }

    double TimingMaxMs(const TimingWindow& window) {
        return window.MaxMilliseconds();
    }
} // namespace highp::metrics::internal
