#pragma once
#include <mutex>
#include <optional>

#include "DeferContext.hpp"

namespace highp::scope {
    template <typename T>
    class DeferredItemHolder {
    public:
        void Hold(DeferContext<T>&& ctx) {
            T* key = ctx.Get();
            std::scoped_lock lock(_mtx);
            _items.emplace(key, std::move(ctx));
        }

        std::optional<DeferContext<T>> Release(T* key) {
            std::scoped_lock lock(_mtx);
            auto it = _items.find(key);
            if (it == _items.end()) {
                return std::nullopt;
            }
            
            auto item = std::move(it->second);
            _items.erase(it);
            return item;
        }

        void Erase(T* key) {
            std::scoped_lock lock(_mtx);
            _items.erase(key);
        }

        template <typename Fn>
        void ForEach(Fn&& fn) {
            std::scoped_lock lock(_mtx);
            for (auto& [key, _] : _items) {
                fn(key);
            }
        }

    private:
        std::unordered_map<T*, DeferContext<T>> _items;
        std::mutex _mtx;
    };
}
