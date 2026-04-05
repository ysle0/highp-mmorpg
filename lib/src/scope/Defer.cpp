#include <pch.h>
#include "inc/scope/Defer.h"

namespace highp::scope {
    // 생성자에 deferFn 을 강제하는 이유는 단순히
    // DeferContextSingle 생성 이후 defer 호출강제를 언어적으로 지원할 수단이 없어서임.
    Defer::Defer(DeferFn deferFn) {
        // 1 개의 defer 정리함수를 기대.
        _deferFns.reserve(1);
        _deferFns.emplace_back(std::move(deferFn));
    }

    Defer::~Defer() noexcept {
        // 등록 역순으로 호출하여 해제
        // 나중에 등록된 작업이 먼저 등록된 자원에 의존할 수 있기 때문임.
        for (auto it = _deferFns.rbegin(); it != _deferFns.rend(); ++it) {
            (*it)();
        }

        _deferFns.clear();
    }

    void Defer::Add(DeferFn deferFn) {
        _deferFns.emplace_back(std::move(deferFn));
    }
}
