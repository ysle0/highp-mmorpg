#pragma once
#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>
#include <concepts>

namespace highp::mem {

    /// Vyukov's bounded MPMC queue 기반 lock-free object pool.
    /// 모든 Get/Return 이 atomic CAS 로 동작하며, mutex 를 사용하지 않는다.
    ///
    /// HybridObjectPool 과의 비교 벤치마크 용도로 작성.
    template <std::default_initializable T>
    class LockFreeObjectPool final {

        // -- Vyukov bounded MPMC ring buffer ----------------------------------
        struct Cell {
            std::atomic<std::size_t> sequence;
            T* data;
        };

        // cacheline padding 으로 false sharing 방지
        static constexpr std::size_t CacheLineSize = 64;

        struct alignas(CacheLineSize) Queue {
            Cell*       cells;
            std::size_t mask;
            char        pad0[CacheLineSize - sizeof(Cell*) - sizeof(std::size_t)];
            alignas(CacheLineSize) std::atomic<std::size_t> enqueuePos;
            alignas(CacheLineSize) std::atomic<std::size_t> dequeuePos;

            explicit Queue(std::size_t capacity) {
                // capacity 는 2의 거듭제곱이어야 한다
                assert((capacity & (capacity - 1)) == 0);
                mask  = capacity - 1;
                cells = new Cell[capacity];
                for (std::size_t i = 0; i < capacity; ++i)
                    cells[i].sequence.store(i, std::memory_order_relaxed);
                enqueuePos.store(0, std::memory_order_relaxed);
                dequeuePos.store(0, std::memory_order_relaxed);
            }

            ~Queue() { delete[] cells; }

            Queue(const Queue&) = delete;
            Queue& operator=(const Queue&) = delete;

            bool TryEnqueue(T* item) {
                Cell* cell;
                std::size_t pos = enqueuePos.load(std::memory_order_relaxed);
                for (;;) {
                    cell = &cells[pos & mask];
                    std::size_t seq = cell->sequence.load(std::memory_order_acquire);
                    auto diff = static_cast<std::ptrdiff_t>(seq) -
                                static_cast<std::ptrdiff_t>(pos);
                    if (diff == 0) {
                        if (enqueuePos.compare_exchange_weak(
                                pos, pos + 1, std::memory_order_relaxed))
                            break;
                    } else if (diff < 0) {
                        return false; // full
                    } else {
                        pos = enqueuePos.load(std::memory_order_relaxed);
                    }
                }
                cell->data = item;
                cell->sequence.store(pos + 1, std::memory_order_release);
                return true;
            }

            bool TryDequeue(T*& item) {
                Cell* cell;
                std::size_t pos = dequeuePos.load(std::memory_order_relaxed);
                for (;;) {
                    cell = &cells[pos & mask];
                    std::size_t seq = cell->sequence.load(std::memory_order_acquire);
                    auto diff = static_cast<std::ptrdiff_t>(seq) -
                                static_cast<std::ptrdiff_t>(pos + 1);
                    if (diff == 0) {
                        if (dequeuePos.compare_exchange_weak(
                                pos, pos + 1, std::memory_order_relaxed))
                            break;
                    } else if (diff < 0) {
                        return false; // empty
                    } else {
                        pos = dequeuePos.load(std::memory_order_relaxed);
                    }
                }
                item = cell->data;
                cell->sequence.store(pos + mask + 1, std::memory_order_release);
                return true;
            }
        };

        // -- Pool state -------------------------------------------------------
        inline static Queue* _queue = nullptr;
        inline static std::atomic<bool> _initialized{false};

        static void EnsureInit(std::size_t capacity = 4096) {
            bool expected = false;
            if (_initialized.compare_exchange_strong(expected, true,
                    std::memory_order_acq_rel)) {
                _queue = new Queue(capacity);
            }
        }

    public:
        LockFreeObjectPool() = delete;

        /// queue 용량을 지정하여 초기화. 벤치마크 시작 전 호출.
        static void Init(std::size_t capacity = 4096) {
            EnsureInit(capacity);
        }

        static T* Get() {
            EnsureInit();
            T* item = nullptr;
            if (_queue->TryDequeue(item))
                return item;
            return new T();
        }

        static void Return(T* item) {
            if (!item) return;
            EnsureInit();
            if (!_queue->TryEnqueue(item))
                delete item; // queue full → 그냥 해제
        }
    };

} // namespace highp::mem
