#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <chrono>
#include <flatbuf/gen/packet_generated.h>
#include "IPacketHandler.hpp"
#include "error/PacketError.h"
#include <logger/Logger.hpp>
#include <concurrency/MPSCCommandQueue.hpp>

namespace highp::net {
    template <typename T>
    concept PayloadType = protocol::PayloadTraits<T>::enum_value != protocol::Payload::NONE;

    /// <summary>
    /// Worker thread에서 logic thread로 전달되는 패킷 커맨드.
    /// FlatBuffer 페이로드를 복사하여 스레드 간 안전하게 전달한다.
    /// </summary>
    struct PacketCommand {
        std::shared_ptr<Client> client;
        protocol::Payload payloadType;
        std::vector<uint8_t> data;
        int64_t enqueuedAtNs = 0;
    };

    struct PacketDispatcherCallbacks {
        std::function<void()> onPacketValidationSucceeded;
        std::function<void()> onPacketValidationFailed;
        std::function<void(size_t count)> onQueuePushed;
        std::function<void(size_t count)> onQueueDrained;
        std::function<void(std::chrono::nanoseconds duration)> onQueueWait;
        std::function<void(std::chrono::nanoseconds duration)> onDispatchProcess;
    };

    class PacketDispatcher {
        using Res = fn::Result<void, err::EPacketError>;
        using DispatchFn = std::function<void(std::shared_ptr<Client>, const protocol::Packet*)>;
        using ParseResult = fn::Result<const protocol::Packet*, err::EPacketError>;

    public:
        explicit PacketDispatcher(std::shared_ptr<log::Logger> logger);

        /// <summary>
        /// 이벤트 콜백을 연결한다.
        /// </summary>
        void UseCallbacks(PacketDispatcherCallbacks callbacks) noexcept;

        /// IOCP worker thread에서 호출: 프레임 조립 + 파싱 + 큐 적재
        void Receive(const std::shared_ptr<Client>& client, std::span<const char> data);

        /// Logic thread에서 호출: 큐에서 커맨드를 꺼내 핸들러에 디스패치함으로 핸들러의 실행 컨텍스트를 logic thread 로 함.
        void Tick();

        /// IPacketHandler<T>를 페이로드 타입별로 등록
        /// std::unique_ptr 로 하지않는 이유 -> 내부 handler 의 lambda 에 handler 캡쳐는 c++23 std::move_only_function
        /// 으로만 달성이 가능하므로 현재로는 불가능!
        template <typename TPayload> requires PayloadType<TPayload>
        void RegisterHandler(std::shared_ptr<IPacketHandler<TPayload>> handler) {
            constexpr protocol::Payload key = protocol::PayloadTraits<TPayload>::enum_value;
            _handlers[key] = [this, handler](
                const std::shared_ptr<Client>& client,
                const protocol::Packet* packet
            ) {
                    const TPayload* payload = packet->payload_as<TPayload>();
                    if (payload == nullptr) {
                        _logger->Warn("[PacketDispatcher] failed to get payload. no handler called.");
                        return;
                    }

                    handler->Handle(client, payload);
                };
        }

    private:
        /// 바이트 버퍼를 검증하고 Packet*으로 역직렬화
        ParseResult ParsePacket(std::span<const char> data) const;

        /// 파싱된 패킷을 핸들러에 디스패치
        Res DispatchPacket(std::shared_ptr<Client> client, const protocol::Packet* packet);

        /// 검증된 페이로드를 복사하여 커맨드 큐에 적재
        void PushCommand(
            std::shared_ptr<Client> client,
            protocol::Payload payloadType,
            std::span<const char> rawPayload);

    private:
        std::shared_ptr<log::Logger> _logger;
        PacketDispatcherCallbacks _callbacks;
        std::unordered_map<protocol::Payload, DispatchFn> _handlers;
        concurrency::MPSCCommandQueue<PacketCommand> _commandQueue;
        std::vector<PacketCommand> _tickBatch;
    };
} // namespace highp::net
