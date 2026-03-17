#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <cstdint>
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
    };

    class PacketDispatcher {
        using Res = fn::Result<void, err::EPacketError>;
        using DispatchFn = std::function<void(std::shared_ptr<Client>, const protocol::Packet*)>;
        using ParseResult = fn::Result<const protocol::Packet*, err::EPacketError>;

    public:
        explicit PacketDispatcher(std::shared_ptr<log::Logger> logger);

        /// IOCP worker thread에서 호출: 프레임 조립 + 파싱 + 큐 적재
        void Receive(std::shared_ptr<Client> client, std::span<const char> data);

        /// Logic thread에서 호출: 큐에서 커맨드를 꺼내 핸들러에 디스패치함으로 핸들러의 실행 컨텍스트를 logic thread 로 함.
        void Tick();

        /// IPacketHandler<T>를 페이로드 타입별로 등록
        template <typename TPayload> requires PayloadType<TPayload>
        void RegisterHandler(IPacketHandler<TPayload>* handler) {
            constexpr protocol::Payload key = protocol::PayloadTraits<TPayload>::enum_value;
            _handlers[key] = [h = std::move(handler)](
                std::shared_ptr<Client> client,
                const protocol::Packet* packet
            ) {
                    const TPayload* payload = packet->payload_as<TPayload>();
                    if (payload == nullptr)
                        return;

                    h->Handle(client, payload);
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
        std::unordered_map<protocol::Payload, DispatchFn> _handlers;
        concurrency::MPSCCommandQueue<PacketCommand> _commandQueue;
        std::vector<PacketCommand> _tickBatch;
    };
} // namespace highp::net
