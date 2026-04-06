#pragma once
#include <functional>
#include <stop_token>
#include <thread>

namespace highp::thread {
    // NOTE: 2026-03-27 std::jthread 자체에 가상 소멸자가 없기 때문에
    // 직접 상속 시 memleak or UB.
    // inheritance -> composition 으로 수정.
    class LogicThread {
        using ExecFn = std::function<void(std::stop_token st)>;

    public:
        void Exec(ExecFn fn);
        void Exit();

    private:
        std::jthread _thread;
    };
}
