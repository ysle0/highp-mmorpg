#include "ResultCollector.h"

#include "metrics/client/ClientMetricsSnapshot.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {
    std::string MakeTimestamp() {
        const auto now = std::chrono::system_clock::now();
        const auto timeT = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;

        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &timeT);
#else
        localtime_r(&timeT, &tm);
#endif

        return std::format("{:04}{:02}{:02}-{:02}{:02}{:02}-{:03}",
                           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                           tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    }

    int GetCurrentPid() {
#ifdef _WIN32
        return static_cast<int>(GetCurrentProcessId());
#else
        return static_cast<int>(getpid());
#endif
    }
} // namespace

std::string ResultCollector::WriteJson(
    const std::string& scenarioName,
    const std::shared_ptr<highp::metrics::IClientMetrics>& metrics,
    int durationSec) {

    std::string dirName = std::format("{}-p{}",
                                      MakeTimestamp(), GetCurrentPid());
    std::filesystem::path dir =
        std::filesystem::path("artifacts") / scenarioName / dirName;

    std::filesystem::create_directories(dir);

    std::filesystem::path filePath = dir / "client-results.json";

    auto snapshot = metrics->TakeSnapshot();

    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, filePath.string().c_str(), "w");
#else
    f = fopen(filePath.string().c_str(), "w");
#endif
    if (!f) {
        return {};
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"scenario_name\": \"%s\",\n", scenarioName.c_str());
    fprintf(f, "  \"connect_success\": %llu,\n", snapshot.connectSuccessTotal);
    fprintf(f, "  \"connect_fail\": %llu,\n", snapshot.connectFailureTotal);
    fprintf(f, "  \"disconnect_total\": %llu,\n", snapshot.disconnectTotal);
    fprintf(f, "  \"send_packets\": %llu,\n", snapshot.sendPacketsTotal);
    fprintf(f, "  \"send_bytes\": %llu,\n", snapshot.sendBytesTotal);
    fprintf(f, "  \"send_fail\": %llu,\n", snapshot.sendFailTotal);
    fprintf(f, "  \"recv_packets\": %llu,\n", snapshot.recvPacketsTotal);
    fprintf(f, "  \"recv_bytes\": %llu,\n", snapshot.recvBytesTotal);
    fprintf(f, "  \"response_timeout\": %llu,\n", snapshot.responseTimeoutTotal);
    fprintf(f, "  \"duration_sec\": %d\n", durationSec);
    fprintf(f, "}\n");

    fclose(f);
    return dir.string();
}
