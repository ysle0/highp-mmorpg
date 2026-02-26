#pragma once

#include <concepts>
#include <list>
#include <functional>

namespace highp::scope {
    template <typename T>
    class DeferContext {
        using DeferFn = std::function<void(T*)>;

    public:
        explicit DeferContext(T* item, DeferFn deferFn)
            : _item(item) {
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
