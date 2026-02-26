#pragma once

#include <concepts>
#include <list>
#include <functional>

namespace highp::scope {
    template <typename T>
    class [[nodiscard]] DeferContext {
        using DeferFn = std::function<void(T*)>;

    public:
        // copy x, move ok
        DeferContext(const DeferContext&) = delete;
        DeferContext& operator=(const DeferContext&) = delete;
        DeferContext(DeferContext&&) = default;
        DeferContext& operator=(DeferContext&&) = default;

        // 생성자에 deferFn 을 강제하는 이유는 단순히
        // DeferContext 생성 이후 defer 호출강제를 언어적으로 지원할 수단이 없어서임.
        explicit DeferContext(T* item, DeferFn deferFn) : _item(item) {
            _deferLists.emplace_back(std::move(deferFn));
        }

        ~DeferContext() {
            if (!_needReturn) {
                return;
            }

            for (auto it = _deferLists.rbegin(); it != _deferLists.rend(); ++it) {
                (*it)(_item);
            }

            _deferLists.clear();
        }

        void PreventReturn() { _needReturn = false; }

        void Defer(DeferFn deferFn) {
            _deferLists.emplace_back(std::move(deferFn));
        }

        T* Get() { return _item; }

    private:
        T* _item;
        std::list<DeferFn> _deferLists;
        bool _needReturn = true;
    };
}
