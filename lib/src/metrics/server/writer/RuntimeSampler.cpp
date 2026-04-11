#include "pch.h"

#include "metrics/server/writer/RuntimeSampler.h"
#include "src/metrics/server/ServerMetricsSupport.h"
#include "scope/DeferContext.hpp"

namespace highp::metrics {
    RuntimeSample RuntimeSampler::Sample() {
        RuntimeSample sample{};
        const auto now = std::chrono::steady_clock::now();

        FILETIME creation{};
        FILETIME exitTime{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exitTime, &kernel, &user)) {
            _hasBaseline = false;
            _lastSampleAt = now;
            _lastProcessKernel100ns = 0;
            _lastProcessUser100ns = 0;
            _lastLogicThreadKernel100ns = 0;
            _lastLogicThreadUser100ns = 0;
            _lastLogicThreadIdSampled = 0;
            return sample;
        }

        const uint64_t processKernel100ns = internal::FileTimeToUint64(kernel);
        const uint64_t processUser100ns = internal::FileTimeToUint64(user);
        const DWORD logicThreadId = _logicThreadId.load(std::memory_order_acquire);

        uint64_t logicKernel100ns = 0;
        uint64_t logicUser100ns = 0;
        bool logicThreadSampled = false;
        if (logicThreadId != 0) {
            HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, logicThreadId);
            if (thread != nullptr) {
                highp::scope::DeferContext<HANDLE> threadCleanup(&thread, [](HANDLE* handle) {
                    if (*handle != nullptr && *handle != INVALID_HANDLE_VALUE) {
                        CloseHandle(*handle);
                    }
                });

                FILETIME threadCreation{};
                FILETIME threadExit{};
                FILETIME threadKernel{};
                FILETIME threadUser{};
                if (GetThreadTimes(thread, &threadCreation, &threadExit, &threadKernel, &threadUser)) {
                    logicKernel100ns = internal::FileTimeToUint64(threadKernel);
                    logicUser100ns = internal::FileTimeToUint64(threadUser);
                    logicThreadSampled = true;
                }
            }
        }

        const uint32_t processorCount = internal::GetProcessorCount();
        if (!_hasBaseline) {
            _hasBaseline = true;
            _lastSampleAt = now;
            _lastProcessKernel100ns = processKernel100ns;
            _lastProcessUser100ns = processUser100ns;
            _lastLogicThreadKernel100ns = logicKernel100ns;
            _lastLogicThreadUser100ns = logicUser100ns;
            _lastLogicThreadIdSampled = logicThreadId;
            return sample;
        }

        const double elapsedSeconds = std::chrono::duration<double>(now - _lastSampleAt).count();
        if (elapsedSeconds > 0.0) {
            const uint64_t processDelta =
                (processKernel100ns - _lastProcessKernel100ns) +
                (processUser100ns - _lastProcessUser100ns);
            const double processSeconds = static_cast<double>(processDelta) * 1e-7;
            sample.processCpuPercent =
                (processSeconds / elapsedSeconds) / static_cast<double>(processorCount) * 100.0;

            if (logicThreadId != 0 && logicThreadSampled) {
                if (_lastLogicThreadIdSampled != logicThreadId) {
                    _lastLogicThreadKernel100ns = logicKernel100ns;
                    _lastLogicThreadUser100ns = logicUser100ns;
                    _lastLogicThreadIdSampled = logicThreadId;
                    sample.logicThreadCpuPercent = 0.0;
                }
                else {
                    const uint64_t logicDelta =
                        logicKernel100ns - _lastLogicThreadKernel100ns +
                        (logicUser100ns - _lastLogicThreadUser100ns);
                    const double logicSeconds = static_cast<double>(logicDelta) * 1e-7;
                    sample.logicThreadCpuPercent =
                        logicSeconds / elapsedSeconds * 100.0;
                }
            }
            else {
                sample.logicThreadCpuPercent = 0.0;
            }
        }

        _lastSampleAt = now;
        _lastProcessKernel100ns = processKernel100ns;
        _lastProcessUser100ns = processUser100ns;
        if (logicThreadSampled) {
            _lastLogicThreadKernel100ns = logicKernel100ns;
            _lastLogicThreadUser100ns = logicUser100ns;
            _lastLogicThreadIdSampled = logicThreadId;
        }

        sample.threadCount = internal::CountProcessThreads();
        return sample;
    }

    void RuntimeSampler::SetLogicThreadId(DWORD threadId) noexcept {
        _logicThreadId.store(threadId, std::memory_order_release);
    }

    void RuntimeSampler::Reset() noexcept {
        _lastSampleAt = {};
        _lastProcessKernel100ns = 0;
        _lastProcessUser100ns = 0;
        _lastLogicThreadKernel100ns = 0;
        _lastLogicThreadUser100ns = 0;
        _lastLogicThreadIdSampled = 0;
        _hasBaseline = false;
    }
} // namespace highp::metrics
