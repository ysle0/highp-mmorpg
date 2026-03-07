#pragma once

#include <functional>
#include <unordered_map>
#include <concepts>
#include <utility>
#include <flatbuffers/flatbuffers.h>
#include <flatbuf/gen/packet_generated.h>
#include "IServerHandler.h"
#include "error/PacketError.h"
#include <logger/Logger.hpp>

namespace highp::net {
    template <typename T>
    concept PayloadType = protocol::PayloadTraits<T>::enum_value != protocol::Payload::NONE;

    template <typename T, typename Fn>
    concept PacketHandlerInvocable = std::invocable<Fn&, std::shared_ptr<Client>, const T*>;

    class PacketHandler : public IServerHandler {
    protected:
        using Res = fn::Result<void, err::EPacketError>;
        using PacketHandlerFn = std::function<void(std::shared_ptr<Client>, const protocol::Packet*)>;
        using ParseResult = fn::Result<const protocol::Packet*, err::EPacketError>;

    public:
        explicit PacketHandler(std::shared_ptr<log::Logger> logger);

    protected:
        void OnAccept(std::shared_ptr<Client> client) override;
        void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override;
        void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) override;
        void OnDisconnect(std::shared_ptr<Client> client) override;

    public:
        /// PayloadTraits<T>::enum_value로 키를 자동 결정하여 핸들러 등록
        template <typename T, typename Fn> requires PayloadType<T>
        void RegisterHandler(Fn&& handler) requires PacketHandlerInvocable<T, Fn> {
            constexpr protocol::Payload key = protocol::PayloadTraits<T>::enum_value;
            _handlers[key] = [fn = std::forward<Fn>(handler)](
                std::shared_ptr<Client> client,
                const protocol::Packet* packet
            ) mutable {
                    const T* payload = packet->payload_as<T>();
                    if (payload == nullptr)
                        return;

                    fn(client, payload);
                };
        }

    private:
        /// 파싱된 패킷을 핸들러에 디스패치
        Res DispatchPacket(std::shared_ptr<Client> client, const protocol::Packet* packet) {
            const auto it = _handlers.find(packet->payload_type());
            if (it == _handlers.end()) {
                _logger->Warn("[PacketHandler] unhandled payload type: {}",
                              protocol::EnumNamePayload(packet->payload_type()));
                return Res::Err(err::EPacketError::FbPayloadTypeNotFound);
            }

            it->second(client, packet);
            return Res::Ok();
        }

        /// 바이트 버퍼를 검증하고 Packet*으로 역직렬화
        ParseResult ParsePacket(std::span<const char> data) const {
            auto verifier = flatbuffers::Verifier(
                reinterpret_cast<const uint8_t*>(data.data()),
                data.size(),
                flatbuffers::VerifierTemplate<false>::Options{
                    .assert = true,
                });

            if (!protocol::VerifyPacketBuffer(verifier)) {
                _logger->Warn("[PacketHandler] failed to verify. invalid packet buffer");
                return ParseResult::Err(err::EPacketError::FbFailedVerify);
            }

            const protocol::Packet* packet = protocol::GetPacket(data.data());
            if (packet == nullptr) {
                _logger->Warn("[PacketHandler] failed to get packet");
                return ParseResult::Err(err::EPacketError::FbGetPacket);
            }

            return ParseResult::Ok(packet);
        }

    protected:
        std::shared_ptr<log::Logger> _logger;

    private:
        std::unordered_map<protocol::Payload, PacketHandlerFn> _handlers;
    };
} // namespace highp::net
