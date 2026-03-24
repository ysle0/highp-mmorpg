#pragma once
#include <functional>

namespace highp::thread {
    class LogicThread {
        using ExecFn = std::function<void(std::stop_token)>;
    public:
        void Exec(ExecFn fn);
        void Exit();

    private:
        std::jthread _thread;
    };
}
