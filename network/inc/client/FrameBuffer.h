#pragma once

#include <array>
#include <span>
#include "config/Const.h"

namespace highp::net {
    /// <summary>
    /// TCP 프레임 조립용 누적 버퍼.
    /// [length:4][payload:N] 프레이밍에서 partial recv를 처리한다.
    /// </summary>
    class FrameBuffer final {
    public:
        /// <summary>수신 데이터를 버퍼 끝에 추가한다.</summary>
        /// <returns>성공 시 true, 오버플로우 시 false</returns>
        bool Append(std::span<const char> data);

        /// <summary>누적된 데이터의 읽기 전용 뷰를 반환한다.</summary>
        std::span<const char> Data() const;

        /// <summary>앞에서 n바이트를 소비하고 나머지를 앞으로 당긴다.</summary>
        void Consume(size_t bytesTransferred);

        /// <summary>현재 누적된 바이트 수</summary>
        size_t Size() const;

    private:
        std::array<char, Const::Buffer::maxFrameSize> _buf{};
        size_t _len = 0;
    };
} // namespace highp::net
