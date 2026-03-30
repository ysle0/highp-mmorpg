#pragma once

#include <functional>

namespace highp::scope {
    /// <summary>RAII 를 활용하여 scope 밖에 나가면 바로 해제되는 정리 도구. go defer 를 이식.</summary>
    /// <remarks></remarks>
    class [[nodiscard]] Defer final {
        using DeferFn = std::function<void()>;

    public:
        void Add(DeferFn deferFn);

    public:
        // 생성자에 deferFn 을 강제하는 이유는 단순히
        // DeferContextSingle 생성 이후 defer 호출강제를 언어적으로 지원할 수단이 없어서임.
        explicit Defer(DeferFn deferFn);
        ~Defer() noexcept;

        // copy x, move x
        Defer(const Defer&) = delete;
        Defer& operator=(const Defer&) = delete;
        Defer(Defer&& from) = delete;
        Defer& operator=(Defer&& from) = delete;

    private:
        std::vector<DeferFn> _deferFns;
    };
}

#define DEFER_CONCAT(a, b) a##b
#define GIVE_TEMP_NAME_TO_DEFER(a, b) DEFER_CONCAT(a, b)
// 임시 이름 붙은 defer 로 변신함.
// e.g. _defer_25, _defer_223, _defer_1
#define DEFER(...) \
    highp::scope::Defer GIVE_TEMP_NAME_TO_DEFER(_defer_, __LINE__)(...)
