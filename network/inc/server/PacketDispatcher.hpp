#pragma once

#include <functional>
#include <unordered_map>
#include <concepts>
#include <utility>
#include <flatbuffers/flatbuffers.h>
#include <flatbuf/gen/packet_generated.h>
#include "IPacketHandler.hpp"
#include "error/PacketError.h"
#include <logger/Logger.hpp>

namespace highp::net {
    template <typename T>
    concept PayloadType = protocol::PayloadTraits<T>::enum_value != protocol::Payload::NONE;

    class PacketDispatcher {
        using Res = fn::Result<void, err::EPacketError>;
        using DispatchFn = std::function<void(std::shared_ptr<Client>, const protocol::Packet*)>;
        using ParseResult = fn::Result<const protocol::Packet*, err::EPacketError>;

    public:
        explicit PacketDispatcher(std::shared_ptr<log::Logger> logger);

        /// Server::OnRecv에서 호출 — 프레임 조립 + 파싱 + 디스패치
        void Handle(std::shared_ptr<Client> client, std::span<const char> data);

        /// IPacketHandler<T>를 페이로드 타입별로 등록
        template <typename T> requires PayloadType<T>
        void RegisterHandler(IPacketHandler<T>* handler) {
            constexpr protocol::Payload key = protocol::PayloadTraits<T>::enum_value;
            _handlers[key] = [handler](
                std::shared_ptr<Client> client,
                const protocol::Packet* packet
            ) {
                    const T* payload = packet->payload_as<T>();
                    if (payload == nullptr)
                        return;

                    handler->Handle(client, payload);
                };
        }

    private:
        /// 파싱된 패킷을 핸들러에 디스패치
        Res DispatchPacket(std::shared_ptr<Client> client, const protocol::Packet* packet);

        /// 바이트 버퍼를 검증하고 Packet*으로 역직렬화
        ParseResult ParsePacket(std::span<const char> data) const;

        std::shared_ptr<log::Logger> _logger;
        std::unordered_map<protocol::Payload, DispatchFn> _handlers;
    };
} // namespace highp::net
