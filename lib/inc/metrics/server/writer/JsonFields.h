#pragma once

#include "src/metrics/server/ServerMetricsSupport.h"

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string_view>

namespace highp::metrics {
    struct JsonTimestamp {
        std::string_view name;
        std::chrono::system_clock::time_point value;

        friend std::ostream& operator<<(std::ostream& os, const JsonTimestamp& f) {
            return os << "\"" << f.name << "\":\""
                      << internal::JsonEscape(internal::FormatUtcTime(f.value)) << "\"";
        }
    };

    struct JsonCounter {
        std::string_view name;
        uint64_t value;

        friend std::ostream& operator<<(std::ostream& os, const JsonCounter& f) {
            return os << "\"" << f.name << "\":" << f.value;
        }
    };

    struct JsonGauge {
        std::string_view name;
        int64_t value;

        friend std::ostream& operator<<(std::ostream& os, const JsonGauge& f) {
            return os << "\"" << f.name << "\":" << f.value;
        }
    };

    struct JsonNumber {
        std::string_view name;
        double value;

        friend std::ostream& operator<<(std::ostream& os, const JsonNumber& f) {
            return os << "\"" << f.name << "\":" << internal::FormatNumber(f.value);
        }
    };

    struct JsonUint {
        std::string_view name;
        uint32_t value;

        friend std::ostream& operator<<(std::ostream& os, const JsonUint& f) {
            return os << "\"" << f.name << "\":" << f.value;
        }
    };

    class JsonObjectWriter {
    public:
        explicit JsonObjectWriter(std::ostream& os) : _os(os) { _os << '{'; }
        ~JsonObjectWriter() { _os << '}'; }

        JsonObjectWriter(const JsonObjectWriter&) = delete;
        JsonObjectWriter& operator=(const JsonObjectWriter&) = delete;

        template<typename T>
        JsonObjectWriter& operator<<(const T& field) {
            if (!_first) { _os << ','; }
            _first = false;
            _os << field;
            return *this;
        }

    private:
        std::ostream& _os;
        bool _first = true;
    };
} // namespace highp::metrics
