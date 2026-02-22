#pragma once

namespace highp::network {
    class ICompletionTarget {
    public:
        virtual ~ICompletionTarget() = default;

        static ICompletionTarget* FromCompletionKey(ULONG_PTR completionKey) noexcept {
            return reinterpret_cast<ICompletionTarget*>(completionKey);
        }

        template <typename T>
            requires std::derived_from<T, ICompletionTarget>
        T* CopyTo() {
            // ULONG_PTR CompletionKey 에 타입 안정성을 추가.
            // ICompletionTarget 을 파라미터로 받는 함수에 컴파일타임으로 타입 강제.
            // see: IocpIoMultiplexer::AssociateSocket()
            return static_cast<T*>(this);
        }

        ULONG_PTR AsCompletionKey() const noexcept { return reinterpret_cast<ULONG_PTR>(this); }
    };
} // namespace highp::network
