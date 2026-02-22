#pragma once
#include <vector>
#include <memory>
#include <concepts>

namespace highp::mem {

template <typename T>
// C# Poolable<T> where T: new(); 와 동일
concept Poolable = std::default_initializable<T>;

template <Poolable T>
class ThreadLocalPool {
	struct LocalStorage {
		std::vector<std::unique_ptr<T>> all;
		std::vector<T*> available;
	};

	static LocalStorage& GetLocalStorage() {
		// 호출자 (스레드)가 자신만의 LocalStorage 1개씩 만을 보유.
		// 같은 함수를 계속 호출해도 스레드마다 1개의 고유한 LocalStorage 객체 반환.
		thread_local LocalStorage s;
		return s;
	}

public:
	T* Acquire() {
		LocalStorage& local = GetLocalStorage();
		if (local.available.empty()) {
			auto p = std::make_unique<T>();
			T* raw = p.get();
			local.all.push_back(std::move(p));

			return raw;
		}

		T* item = local.available.back();
		local.available.pop_back();

		return item;
	}

	void Release(T* item) {
		if (!item) {
			return;
		}

		LocalStorage& local = GetLocalStorage();

		local.available.push_back(item);
	}

	void Clear() {
		LocalStorage& local = GetLocalStorage();
		local.available.clear();
		local.all.clear();
	}


	size_t GetAvailableCount() const {
		LocalStorage& s = GetLocalStorage();
		return s.available.size();
	}

	size_t GetTotalCount() const {
		LocalStorage& s = GetLocalStorage();
		return s.all.size();
	}
};
} // namespace highp::mem