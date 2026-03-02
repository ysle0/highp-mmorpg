#pragma once

#include <concepts>
#include <list>
#include <functional>

namespace highp::scope {
    template <typename T>
    class [[nodiscard]] DeferContext {
        using DeferFn = std::function<void(T*)>;

    public:
        // 생성자에 deferFn 을 강제하는 이유는 단순히
        // DeferContext 생성 이후 defer 호출강제를 언어적으로 지원할 수단이 없어서임.
        explicit DeferContext(T* item, DeferFn deferFn) : _item(item) {
            // 2-3 개의 defer 정리함수를 기대.
            _deferFns.reserve(3);
            _deferFns.emplace_back(std::move(deferFn));
        }

        ~DeferContext() {
            if (!_needReturn) {
                return;
            }

            for (auto it = _deferFns.rbegin(); it != _deferFns.rend(); ++it) {
                (*it)(_item);
            }

            _deferFns.clear();
        }


        // copy x, move ok
        DeferContext(const DeferContext&) = delete;
        DeferContext& operator=(const DeferContext&) = delete;

        DeferContext(DeferContext&& from) noexcept : _item(nullptr) {
            MoveFrom(std::forward<DeferContext&&>(from));
        }

        DeferContext& operator=(DeferContext&& from) noexcept {
            return MoveFrom(std::forward<DeferContext&&>(from));
        }

        DeferContext& MoveFrom(DeferContext&& from) {
            _item = from._item;
            from._item = nullptr;

            _deferFns = std::move(from._deferFns);

            _needReturn = from._needReturn;
            from._needReturn = false;

            return *this;
        }

        void Defer(DeferFn deferFn) {
            _deferFns.emplace_back(std::move(deferFn));
        }

        T* Get() { return _item; }

    private:
        T* _item;
        std::vector<DeferFn> _deferFns;
        bool _needReturn = true;
    };
}
