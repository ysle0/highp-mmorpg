#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <error/NetworkError.h>
#include <functional/Result.hpp>

namespace highp::network {
    class TcpClientSocket;

    /// <summary>
    /// TCP 바이트 스트림 위에 길이 기반 프레이밍(uint32 + payload)을 제공한다.
    /// </summary>
    class PacketStream final {
    public:
        using Res = fn::Result<void, err::ENetworkError>;
        using ResWithSize = fn::Result<size_t, err::ENetworkError>;

        /// <summary>
        /// 특정 소켓 인스턴스에 바인딩된 프레이머를 생성한다.
        /// </summary>
        explicit PacketStream(TcpClientSocket& socket, size_t maxFrameSize = 64 * 1024);

        /// <summary>
        /// [length(4)][payload] 형태로 1개 프레임을 전송한다.
        /// </summary>
        Res SendFrame(std::span<const uint8_t> payload);

        /// <summary>
        /// 1개 프레임을 수신해 payload만 outPayload로 반환한다.
        /// </summary>
        ResWithSize RecvFrame(std::vector<uint8_t>& outPayload);

    private:
        /// <summary>
        /// 지정 바이트 수를 모두 채울 때까지 반복 수신한다.
        /// </summary>
        Res RecvExact(std::span<char> buffer);

        /// <summary>프레임 송수신 대상 TCP 소켓</summary>
        TcpClientSocket& _socket;

        /// <summary>허용 가능한 최대 payload 크기(안전장치)</summary>
        size_t _maxFrameSize;
    };
}
