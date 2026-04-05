#include "pch.h"
#include <array>
#include "client/PacketStream.h"
#include "client/TcpClientSocket.h"

namespace highp::net {
    PacketStream::PacketStream(TcpClientSocket& socket, size_t maxFrameSize)
    // 소켓은 외부에서 관리하며, PacketStream은 참조만 보관한다.
        : _socket(socket)
          , _maxFrameSize(maxFrameSize) {
        //
    }

    PacketStream::Res PacketStream::SendFrame(std::span<const uint8_t> payload) {
        // [length:4 byte][payload:N byte] 연속 메모리 버퍼를 만든다.
        uint32_t payloadSize = static_cast<uint32_t>(payload.size());
        std::vector<char> frame(kHeaderSize + payload.size(), 0);

        // 네트워크 바이트 오더 (패킷 헤더는 big-endian 으로 쓴다.)
        payloadSize = htonl(payloadSize);
        // 헤더에 payload 길이를 기록한다.
        std::memcpy(frame.data(), &payloadSize, kHeaderSize);

        // payload가 있으면 헤더 뒤에 복사한다.
        if (payloadSize > 0) {
            std::memcpy(frame.data() + kHeaderSize, payload.data(), payload.size());
        }

        // 소켓 raw 전송은 TcpClientSocket이 책임진다.
        return _socket.SendAll(std::span<const char>{frame.data(), frame.size()});
    }

    PacketStream::ResWithSize PacketStream::RecvFrame(std::vector<uint8_t>& outPayload) {
        // 프레임 최대 크기 설정이 비정상인지 먼저 검증한다.
        if (_maxFrameSize == 0) {
            return ResWithSize::Err(err::ENetworkError::WsaInvalidArgs);
        }

        if (_maxFrameSize > Const::Buffer::maxFrameSize) {
            return ResWithSize::Err(err::ENetworkError::WsaInvalidArgs);
        }

        // 1) 길이 헤더(4바이트)를 정확히 수신한다.
        std::array<char, kHeaderSize> buf{};
        const auto span = std::span{buf.data(), buf.size()};
        if (const Res recvHeaderRes = RecvExact(span);
            recvHeaderRes.HasErr()) {
            return ResWithSize::Err(recvHeaderRes.Err());
        }

        // 2) 헤더에서 payload 길이를 읽는다.
        uint32_t payloadSize = 0;
        std::memcpy(&payloadSize, buf.data(), kHeaderSize);

        payloadSize = ntohl(payloadSize);

        // 3) DoS/메모리 폭주 방지를 위해 최대 크기 제한을 적용한다.
        if (payloadSize > _maxFrameSize) {
            return ResWithSize::Err(err::ENetworkError::WsaNoBufferSpace);
        }

        // 4) payload 크기만큼 출력 버퍼를 준비한다.
        outPayload.resize(payloadSize);
        if (payloadSize == 0) {
            return ResWithSize::Ok(0);
        }

        // 5) payload 본문을 정확히 수신한다.
        auto payloadSpan = std::span{
            reinterpret_cast<char*>(outPayload.data()),
            outPayload.size()
        };

        if (const Res recvPayloadRes = RecvExact(payloadSpan);
            recvPayloadRes.HasErr()) {
            return ResWithSize::Err(recvPayloadRes.Err());
        }

        // 반환값은 실제 payload 크기.
        return ResWithSize::Ok(payloadSize);
    }

    PacketStream::Res PacketStream::RecvExact(std::span<char> buffer) {
        // TCP는 partial recv가 일반적이므로 목표 바이트까지 반복 수신한다.
        size_t receivedBytes = 0;
        while (receivedBytes < buffer.size()) {
            const ResWithSize recvRes = _socket.RecvSome(buffer.subspan(receivedBytes));
            if (recvRes.HasErr()) {
                return Res::Err(recvRes.Err());
            }

            const size_t chunkSize = recvRes.Data();
            // 0바이트 수신은 보통 peer close로 간주한다.
            if (chunkSize == 0) {
                return Res::Err(err::ENetworkError::SocketInvalid);
            }

            receivedBytes += chunkSize;
        }

        return Res::Ok();
    }
}
