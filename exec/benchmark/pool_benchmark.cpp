/// pool_benchmark.cpp
/// HybridObjectPool (TLS + mutex global) vs LockFreeObjectPool (MPMC queue) 성능 비교.
///
/// 측정 항목:
///   1. Single-thread throughput (Get/Return 쌍)
///   2. Multi-thread throughput (2, 4, 8, 16 threads)
///   3. Producer-heavy / Consumer-heavy 비대칭 워크로드
///   4. Cross-thread transfer (Thread A 에서 Get, Thread B 에서 Return)
///
/// Build (Linux/g++):
///   g++ -std=c++20 -O2 -pthread -I../../lib/inc pool_benchmark.cpp -o pool_benchmark
///
/// Build (MSVC):
///   cl /std:c++20 /O2 /EHsc /I..\..\lib\inc pool_benchmark.cpp

#include <memory/HybridObjectPool.hpp>
#include <memory/LockFreeObjectPool.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// 풀링 대상 더미 객체 — IOCP OverlappedExt 와 비슷한 크기
// ---------------------------------------------------------------------------
struct PoolableObject {
    char payload[256]{};
};

// ---------------------------------------------------------------------------
// 유틸
// ---------------------------------------------------------------------------
using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::milli>;

static constexpr int Warmup      = 5'000;
static constexpr int OpsPerThread = 500'000;

struct BenchResult {
    double totalMs;        // 전체 경과 시간
    double opsPerSec;      // 전체 throughput
    double avgNsPerOp;     // 연산 1회 평균 ns
};

static void PrintHeader(const char* title) {
    std::printf("\n====== %s ======\n", title);
    std::printf("%-28s %12s %14s %12s\n",
                "Scenario", "Time(ms)", "ops/sec", "ns/op");
    std::printf("--------------------------------------------------------------\n");
}

static void PrintRow(const char* label, const BenchResult& r) {
    std::printf("%-28s %12.2f %14.0f %12.1f\n",
                label, r.totalMs, r.opsPerSec, r.avgNsPerOp);
}

// ---------------------------------------------------------------------------
// 벤치마크 1: 단일 스레드 Get/Return throughput
// ---------------------------------------------------------------------------
template <typename PoolTag>
BenchResult BenchSingleThread(int ops) {
    // 웜업
    for (int i = 0; i < Warmup; ++i) {
        auto* p = PoolTag::Get();
        PoolTag::Return(p);
    }

    auto start = Clock::now();
    for (int i = 0; i < ops; ++i) {
        auto* p = PoolTag::Get();
        PoolTag::Return(p);
    }
    auto end = Clock::now();

    double ms = Duration(end - start).count();
    double totalOps = static_cast<double>(ops);
    return {ms, totalOps / (ms / 1000.0), (ms * 1'000'000.0) / totalOps};
}

// ---------------------------------------------------------------------------
// 벤치마크 2: 멀티 스레드 — 각 스레드가 독립적으로 Get/Return
// ---------------------------------------------------------------------------
template <typename PoolTag>
BenchResult BenchMultiThread(int threadCount, int opsPerThread) {
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    auto worker = [&]() {
        // 웜업
        for (int i = 0; i < Warmup; ++i) {
            auto* p = PoolTag::Get();
            PoolTag::Return(p);
        }
        ready.fetch_add(1, std::memory_order_release);
        while (!go.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (int i = 0; i < opsPerThread; ++i) {
            auto* p = PoolTag::Get();
            PoolTag::Return(p);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int i = 0; i < threadCount; ++i)
        threads.emplace_back(worker);

    // 모든 스레드 준비 대기
    while (ready.load(std::memory_order_acquire) < threadCount)
        std::this_thread::yield();

    auto start = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    auto end = Clock::now();

    double ms = Duration(end - start).count();
    double totalOps = static_cast<double>(threadCount) * opsPerThread;
    return {ms, totalOps / (ms / 1000.0), (ms * 1'000'000.0) / totalOps};
}

// ---------------------------------------------------------------------------
// 벤치마크 3: Cross-thread transfer
//   절반 스레드가 Get (producer), 나머지가 Return (consumer)
//   공유 atomic 배열을 통해 객체 전달 — 실제 IOCP에서 Accept 스레드 → Worker 스레드 패턴
// ---------------------------------------------------------------------------
template <typename PoolTag>
BenchResult BenchCrossThread(int threadCount, int totalOps) {
    // 객체 전달용 lock-free 단방향 채널 (단순 atomic 슬롯 배열)
    static constexpr int SlotCount = 4096;
    std::atomic<PoolableObject*> slots[SlotCount];
    for (auto& s : slots) s.store(nullptr, std::memory_order_relaxed);

    int producerCount = threadCount / 2;
    int consumerCount = threadCount - producerCount;
    int opsPerProducer = totalOps / producerCount;

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::atomic<int> producersDone{0};
    std::atomic<int> consumed{0};

    auto producer = [&](int id) {
        ready.fetch_add(1);
        while (!go.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (int i = 0; i < opsPerProducer; ++i) {
            auto* p = PoolTag::Get();
            // 빈 슬롯 찾아서 넣기
            int slot = (id * 997 + i) % SlotCount;
            for (int attempt = 0; attempt < SlotCount; ++attempt) {
                int idx = (slot + attempt) % SlotCount;
                PoolableObject* expected = nullptr;
                if (slots[idx].compare_exchange_weak(expected, p,
                        std::memory_order_release, std::memory_order_relaxed))
                    goto placed;
            }
            // 슬롯 다 차면 직접 반환
            PoolTag::Return(p);
            placed:;
        }
        producersDone.fetch_add(1, std::memory_order_release);
    };

    auto consumer = [&]() {
        ready.fetch_add(1);
        while (!go.load(std::memory_order_acquire))
            std::this_thread::yield();

        while (true) {
            bool anyProducerAlive = producersDone.load(std::memory_order_acquire) < producerCount;
            bool found = false;
            for (int idx = 0; idx < SlotCount; ++idx) {
                PoolableObject* p = slots[idx].load(std::memory_order_acquire);
                if (p && slots[idx].compare_exchange_weak(p, nullptr,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    PoolTag::Return(p);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    found = true;
                }
            }
            if (!anyProducerAlive && !found) break;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < producerCount; ++i)
        threads.emplace_back(producer, i);
    for (int i = 0; i < consumerCount; ++i)
        threads.emplace_back(consumer);

    while (ready.load() < threadCount)
        std::this_thread::yield();

    auto start = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    auto end = Clock::now();

    double ms = Duration(end - start).count();
    double ops = static_cast<double>(producerCount) * opsPerProducer;
    return {ms, ops / (ms / 1000.0), (ms * 1'000'000.0) / ops};
}

// ---------------------------------------------------------------------------
// 벤치마크 4: Burst 패턴 — Get N개 → Return N개 (batch)
//   Accept burst 시뮬레이션: 한꺼번에 많은 객체를 꺼낸 뒤 나중에 반환
// ---------------------------------------------------------------------------
template <typename PoolTag>
BenchResult BenchBurst(int threadCount, int opsPerThread, int burstSize) {
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    auto worker = [&]() {
        std::vector<PoolableObject*> batch;
        batch.reserve(burstSize);

        ready.fetch_add(1);
        while (!go.load(std::memory_order_acquire))
            std::this_thread::yield();

        int remaining = opsPerThread;
        while (remaining > 0) {
            int count = std::min(burstSize, remaining);
            // burst Get
            for (int i = 0; i < count; ++i)
                batch.push_back(PoolTag::Get());
            // burst Return
            for (auto* p : batch)
                PoolTag::Return(p);
            batch.clear();
            remaining -= count;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i)
        threads.emplace_back(worker);

    while (ready.load() < threadCount)
        std::this_thread::yield();

    auto start = Clock::now();
    go.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    auto end = Clock::now();

    double ms = Duration(end - start).count();
    double totalOps = static_cast<double>(threadCount) * opsPerThread;
    return {ms, totalOps / (ms / 1000.0), (ms * 1'000'000.0) / totalOps};
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
using Hybrid   = highp::mem::HybridObjectPool<PoolableObject>;
using LockFree = highp::mem::LockFreeObjectPool<PoolableObject>;

int main() {
    std::printf("Pool Benchmark: HybridObjectPool (TLS+Lock) vs LockFreeObjectPool (MPMC)\n");
    std::printf("Object size: %zu bytes, Ops/thread: %d\n\n",
                sizeof(PoolableObject), OpsPerThread);

    unsigned hwThreads = std::thread::hardware_concurrency();
    std::printf("Hardware threads: %u\n", hwThreads);

    // LockFree pool 초기화 (충분한 크기)
    LockFree::Init(8192);

    // ---- 1. Single thread ----
    {
        PrintHeader("1. Single Thread Get/Return");
        auto h = BenchSingleThread<Hybrid>(OpsPerThread);
        auto l = BenchSingleThread<LockFree>(OpsPerThread);
        PrintRow("Hybrid (TLS+Lock)", h);
        PrintRow("LockFree (MPMC)", l);
        std::printf("  => Hybrid is %.2fx %s\n",
                     h.opsPerSec > l.opsPerSec
                         ? h.opsPerSec / l.opsPerSec
                         : l.opsPerSec / h.opsPerSec,
                     h.opsPerSec > l.opsPerSec ? "faster" : "slower");
    }

    // ---- 2. Multi-thread scalability ----
    {
        PrintHeader("2. Multi-Thread Scalability (Get/Return per thread)");
        for (int n : {2, 4, 8}) {
            if (n > static_cast<int>(hwThreads * 2)) break;
            char label[64];

            std::snprintf(label, sizeof(label), "Hybrid  %2d threads", n);
            auto h = BenchMultiThread<Hybrid>(n, OpsPerThread);
            PrintRow(label, h);

            std::snprintf(label, sizeof(label), "LockFree %2d threads", n);
            auto l = BenchMultiThread<LockFree>(n, OpsPerThread);
            PrintRow(label, l);

            std::printf("  => Hybrid is %.2fx %s\n",
                         h.opsPerSec > l.opsPerSec
                             ? h.opsPerSec / l.opsPerSec
                             : l.opsPerSec / h.opsPerSec,
                         h.opsPerSec > l.opsPerSec ? "faster" : "slower");
        }
    }

    // ---- 3. Cross-thread transfer ----
    {
        PrintHeader("3. Cross-Thread Transfer (Get on A, Return on B)");
        for (int n : {4, 8}) {
            if (n > static_cast<int>(hwThreads * 2)) break;
            char label[64];
            int totalOps = OpsPerThread * (n / 2);

            std::snprintf(label, sizeof(label), "Hybrid  %2d threads", n);
            auto h = BenchCrossThread<Hybrid>(n, totalOps);
            PrintRow(label, h);

            std::snprintf(label, sizeof(label), "LockFree %2d threads", n);
            auto l = BenchCrossThread<LockFree>(n, totalOps);
            PrintRow(label, l);

            std::printf("  => Hybrid is %.2fx %s\n",
                         h.opsPerSec > l.opsPerSec
                             ? h.opsPerSec / l.opsPerSec
                             : l.opsPerSec / h.opsPerSec,
                         h.opsPerSec > l.opsPerSec ? "faster" : "slower");
        }
    }

    // ---- 4. Burst pattern ----
    {
        PrintHeader("4. Burst Pattern (Get N then Return N)");
        int n = std::min(4u, hwThreads);
        for (int burst : {10, 50, 200}) {
            char label[64];

            std::snprintf(label, sizeof(label), "Hybrid  burst=%3d", burst);
            auto h = BenchBurst<Hybrid>(n, OpsPerThread, burst);
            PrintRow(label, h);

            std::snprintf(label, sizeof(label), "LockFree burst=%3d", burst);
            auto l = BenchBurst<LockFree>(n, OpsPerThread, burst);
            PrintRow(label, l);

            std::printf("  => Hybrid is %.2fx %s\n",
                         h.opsPerSec > l.opsPerSec
                             ? h.opsPerSec / l.opsPerSec
                             : l.opsPerSec / h.opsPerSec,
                         h.opsPerSec > l.opsPerSec ? "faster" : "slower");
        }
    }

    std::printf("\n[Done]\n");
    return 0;
}
