#pragma once

#include <mutex>
#include <vector>
#include <utility>

namespace highp::concurrency {
    /// <summary>
    /// MPSC (Multiple Producer, Single Consumer) 커맨드 큐.
    /// double-buffer swap 방식으로 lock 점유 시간을 최소화한다.
    /// HybridObjectPool 을 생각하며 context switching, lock contention 을 최소화 하는 방향이
    /// 구현 난이도를 줄이고 가성비있게 성능을 확보할 수 있는 방향이라고 생각헸음.
    /// </summary>
    template <typename T>
    class MPSCCommandQueue {
    public:
        /// Producer: 커맨드를 큐에 추가 (IOCP worker thread에서 호출)
        void Push(T&& item) {
            std::scoped_lock lock(_mutex);
            _writeQueue.push_back(std::move(item));
        }

        /// Consumer: 큐 전체를 out으로 swap하여 비운다 (logic thread에서 호출)
        /// swap 후 out을 lock 없이 순회 처리한다.
        void DrainTo(std::vector<T>& out) {
            out.clear();

            std::scoped_lock lock(_mutex);

            // swap 으로 포인터 연산 3번만으로 간단히 해결.
            std::swap(_writeQueue, out);
        }

        bool Empty() const {
            std::scoped_lock lock(_mutex);
            return _writeQueue.empty();
        }

    private:
        std::mutex _mutex;
        std::vector<T> _writeQueue;
    };
}
