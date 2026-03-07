
#pragma once

#include <concepts>
#include <functional>
#include <list>
#include <utility>

namespace highp::scope {
class Defer {
	using DeferFn = std::function<void()>;

public:
	// 생성자에 deferFn 을 강제하는 이유는 단순히
	// DeferContextSingle 생성 이후 defer 호출강제를 언어적으로 지원할 수단이 없어서임.
	explicit Defer(DeferFn deferFn) {
		// 2-3 개의 defer 정리함수를 기대.
		_deferFns.reserve(3);
		_deferFns.emplace_back(std::move(deferFn));
	}

	~Defer() {
		if (!_needReturn) {
			return;
		}

		for (auto it = _deferFns.rbegin(); it != _deferFns.rend(); ++it) {
			(*it)();
		}

		_deferFns.clear();
	}


	// copy x, move ok
	Defer(const Defer&) = delete;
	Defer& operator=(const Defer&) = delete;

	Defer(Defer&& from) noexcept {
		MoveFrom(std::forward<Defer&&>(from));
	}

	Defer& operator=(Defer&& from) noexcept {
		return MoveFrom(std::forward<Defer&&>(from));
	}

	Defer& MoveFrom(Defer&& from) {
		_deferFns = std::move(from._deferFns);

		_needReturn = from._needReturn;
		from._needReturn = false;

		return *this;
	}

	void Defer(DeferFn deferFn) {
		_deferFns.emplace_back(std::move(deferFn));
	}

private:
	std::vector<DeferFn> _deferFns;
	bool _needReturn = true;
};
}
