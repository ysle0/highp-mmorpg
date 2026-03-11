#include "pch.h"
#include "LogicThread.h"

namespace highp::thread {
    LogicThread LogicThread::Exec(ExecFn fn) {
        return LogicThread(
            [_fn=std::move(fn)](std::stop_token st) {
                _fn(std::move(st));
            });
    }

    LogicThread::LogicThread(ExecFn fn) : std::jthread(std::move(fn)) {
        //
    }
}
