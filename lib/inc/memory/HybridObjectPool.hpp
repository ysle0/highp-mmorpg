#pragma once

#include <concepts>
#include <mutex>
#include <vector>

#include "scope/DeferContext.hpp"

namespace highp::mem {
    struct HybridObjectPoolConfig {
        static constexpr int LocalMaxCapacity = 50 * 2;
        static constexpr int ChunkSize = 50;
    };

    template <typename T>
    concept Poolable = std::default_initializable<T>;

    template <Poolable T>
    class HybridObjectPool final {
        inline static std::mutex _globalMtx;
        inline static std::vector<T*> _globalPool;

        struct LocalStorage {
            std::vector<T*> pool;

            LocalStorage() {
                pool.reserve(HybridObjectPoolConfig::LocalMaxCapacity);
            }

            ~LocalStorage() {
                if (pool.empty()) return;

                std::lock_guard lock(_globalMtx);

                // return back
                for (int i = 0; i < pool.size(); i++) {
                    T* lend = pool[i];
                    _globalPool.emplace_back(lend);
                }
                pool.clear();
            }
        };

        static LocalStorage& GetLocalStorage() {
            thread_local LocalStorage local;
            return local;
        }

    public:
        // static only
        HybridObjectPool() = delete;

        static scope::DeferContext<T> Get() {
            LocalStorage& local = GetLocalStorage();

            // thread local 에 있으면 바로 꺼내줌 (lock-free)
            if (!local.pool.empty()) {
                T* item = local.pool.back();
                local.pool.pop_back();

                return scope::DeferContext<T>{
                    item,
                    [](T* x) { HybridObjectPool::Return(x); }
                };
            }

            // 없으면 chunk 하나 때옴
            {
                std::lock_guard lock(_globalMtx);

                // global 에 여유가 있으면 가져옴
                if (const int size = static_cast<int>(_globalPool.size()); size > 0) {
                    const int fetchCount = min(size, HybridObjectPoolConfig::ChunkSize);
                    auto from = _globalPool.end() - fetchCount;
                    local.pool.insert(local.pool.end(), from, _globalPool.end());
                    _globalPool.erase(from, _globalPool.end());
                }
            }

            // global 에서 꺼내도 여전히 없으면 새로 만들어서 바로 반환
            // 어짜피 이후 RAII 로 Pool 에 다시 돌아옴.
            if (local.pool.empty()) {
                return scope::DeferContext<T>{
                    new T(),
                    [](T* x) { HybridObjectPool::Return(x); }
                };
            }

            T* item = local.pool.back();
            local.pool.pop_back();

            return scope::DeferContext<T>{
                item,
                [](T* x) { HybridObjectPool::Return(x); }
            };
        }

        static void Return(T* item) {
            if (!item) return;

            LocalStorage& local = GetLocalStorage();

            local.pool.push_back(item);

            if (local.pool.size() >= HybridObjectPoolConfig::LocalMaxCapacity) {
                std::lock_guard lock(_globalMtx);

                constexpr int move = HybridObjectPoolConfig::ChunkSize;
                auto from = local.pool.end() - move;

                _globalPool.insert(_globalPool.end(), from, local.pool.end());
                local.pool.erase(from, local.pool.end());
            }
        }

        /// global pool 을 안전하게 해제하려면 여기서 chunk 를 때간
        /// thread local.Pool.들을 모두 join 시켜 소멸자에서 전부 global pool 로 빌려간 object들을 돌려줘야함
        /// -> 규약으로 모든 스레드들을 join..
        /// 
        /// 애초에 프로세스 종료됨에 따라 OS가 메모리 회수해갈거고, 프로세스가 살아있으면서
        /// pool 을 초기화 하는 시나리오..? 게임서버에서 그런 경우가 있을지 모르겠음.
    };
}
