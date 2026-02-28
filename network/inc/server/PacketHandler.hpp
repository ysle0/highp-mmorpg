#pragma once

#include <functional>
#include <unordered_map>
#include <flatbuffers/flatbuffers.h>
#include <flatbuf/gen/packet_generated.h>

#include "IServerHandler.h"
#include "error/PacketError.h"

namespace highp::log {
    class Logger;
}

namespace highp::net {
    class PacketHandler : public IServerHandler {
    public:
        explicit PacketHandler(std::shared_ptr<log::Logger> logger);

    private:
        void OnAccept(std::shared_ptr<Client> client) override;
        void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override;
        void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) override;
        void OnDisconnect(std::shared_ptr<Client> client) override;

    protected:
        // type-erased handler: (shared_ptr<Client>, const protocol::Packet* payload)
        using PacketHandlerFn = std::function<void(std::shared_ptr<Client>, const protocol::Packet*)>;
        using ParseResult = fn::Result<const protocol::Packet*, err::EPacketError>;
        using Res = fn::Result<void, err::EPacketError>;

        /// PayloadTraits<T>::enum_value로 키를 자동 결정하여 핸들러 등록
        template <typename T, typename Fn>
        void RegisterHandler(Fn&& handler) {
            constexpr auto key = protocol::PayloadTraits<T>::enum_value;
            _handlers[key] = std::forward<Fn>(handler);
        }

        /// 바이트 버퍼를 검증하고 Packet*으로 역직렬화
        ParseResult ParsePacket(std::span<const char> data);

        /// 파싱된 패킷을 핸들러에 디스패치
        Res DispatchPacket(std::shared_ptr<Client> client, const protocol::Packet* packet);

    private:
        std::shared_ptr<log::Logger> _logger;
        std::unordered_map<protocol::Payload, PacketHandlerFn> _handlers;
    };
} // namespace highp::net
