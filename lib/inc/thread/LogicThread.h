#pragma once
#include <functional>

namespace highp::thread {
    class LogicThread : public std::jthread {
        using ExecFn = std::function<void(std::stop_token)>;
    public:
        LogicThread() = default;
        static LogicThread Exec(ExecFn fn);

    private:
        explicit LogicThread(ExecFn fn);
    };
}
