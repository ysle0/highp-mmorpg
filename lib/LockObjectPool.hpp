#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <concepts>

namespace highp::mem {

template <typename T>
concept Poolable = std::default_initializable<T>;

/// <summary>
/// 범용 객체 풀 템플릿.
/// 객체 재사용을 통해 빈번한 할당/해제 오버헤드를 줄인다.
/// </summary>
/// <typeparam name="T">풀링할 객체 타입. 기본 생성자 필요.</typeparam>
/// <remarks>
/// 2컨테이너 방식: _all이 소유권 유지, _available이 사용 가능 객체 추적.
/// 스레드 안전: mutex로 Acquire/Release 동기화.
/// </remarks>
template <Poolable T>
class LockObjectPool final {
public:
	/// <summary>
	/// LockObjectPool 생성자.
	/// </summary>
	/// <param name="preAllocCount">사전 할당할 객체 수. 기본값 0.</param>
	explicit LockObjectPool(int preAllocCount = 0) {
		PreAllocate(preAllocCount);
	}

	~LockObjectPool() = default;

	LockObjectPool(const LockObjectPool&) = delete;
	LockObjectPool& operator=(const LockObjectPool&) = delete;
	LockObjectPool(LockObjectPool&&) = delete;
	LockObjectPool& operator=(LockObjectPool&&) = delete;

	/// <summary>
	/// 객체를 사전 할당한다.
	/// </summary>
	/// <param name="count">할당할 객체 수</param>
	void PreAllocate(int count) {
		std::lock_guard<std::mutex> lock(_mutex);
		for (int i = 0; i < count; ++i) {
			auto ptr = std::make_shared<T>();
			_available.push_back(ptr.get());
			_all.push_back(std::move(ptr));
		}
	}

	/// <summary>
	/// 풀에서 객체를 획득한다.
	/// 사용 가능한 객체가 없으면 새로 생성한다.
	/// </summary>
	/// <returns>객체의 raw pointer</returns>
	T* Acquire() {
		std::lock_guard<std::mutex> lock(_mutex);
		if (_available.empty()) {
			auto ptr = std::make_shared<T>();
			T* raw = ptr.get();
			_all.push_back(std::move(ptr));
			return raw;
		}
		T* obj = _available.back();
		_available.pop_back();
		return obj;
	}

	/// <summary>
	/// 객체를 풀에 반환한다.
	/// </summary>
	/// <param name="obj">반환할 객체의 raw pointer</param>
	void Release(T* obj) {
		if (!obj) return;
		std::lock_guard<std::mutex> lock(_mutex);
		_available.push_back(obj);
	}

	/// <summary>
	/// 풀을 정리한다. 모든 객체가 해제된다.
	/// </summary>
	void Clear() {
		std::lock_guard<std::mutex> lock(_mutex);
		_available.clear();
		_all.clear();
	}

	/// <summary>현재 풀에서 사용 가능한 객체 수</summary>
	size_t AvailableCount() {
		std::lock_guard<std::mutex> lock(_mutex);
		return _available.size();
	}

	/// <summary>풀이 소유한 총 객체 수</summary>
	size_t TotalCount() {
		std::lock_guard<std::mutex> lock(_mutex);
		return _all.size();
	}

private:
	/// <summary>모든 객체 소유 (영구)</summary>
	std::vector<std::shared_ptr<T>> _all;

	/// <summary>사용 가능한 객체 추적</summary>
	std::vector<T*> _available;

	/// <summary>풀 접근 동기화</summary>
	std::mutex _mutex;
};

} // namespace highp::mem
