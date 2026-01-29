#pragma once

namespace highp::network {

struct ICompletionTarget {
	virtual ~ICompletionTarget() = default;

	template <typename T>
	  requires std::derived_from<T, ICompletionTarget>
	T* CastTo() {
		// ULONG_PTR CompletionKey 에 타입 안정성 추가!
		// ICompletionTarget 을 파라미터로 받는 함수에 concepts 로 std::is_derived_from<T, Derived> 검파일 타임 강제.
		// see: IoCompletionPort::AssociateSocket();
		return static_cast<T*>(this);
	}
};
}
