#include "pch.h"
#include "LogicThread.h"

namespace highp::thread {
    void LogicThread::Exec(ExecFn fn) {
        Exit();
        _thread = std::jthread(std::move(fn));
    }

    void LogicThread::Exit() {
        if (_thread.joinable()) {
            _thread.request_stop();
            _thread.join();
        }
    }
}
