#pragma once

#include <functional>

namespace highp::scope {
    class Defer {
        using DeferFn = std::function<void()>;

    public:
        void Add(DeferFn deferFn);

    public:
        // 생성자에 deferFn 을 강제하는 이유는 단순히
        // DeferContextSingle 생성 이후 defer 호출강제를 언어적으로 지원할 수단이 없어서임.
        explicit Defer(DeferFn deferFn);
        ~Defer() noexcept;
        
        // copy x, move ok
        Defer(const Defer&) = delete;
        Defer& operator=(const Defer&) = delete;
        Defer(Defer&& from) noexcept = default;
        Defer& operator=(Defer&& from) noexcept = default;

    private:
        std::vector<DeferFn> _deferFns;
    };
}
