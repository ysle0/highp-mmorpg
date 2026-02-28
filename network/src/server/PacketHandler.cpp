#include "pch.h"
#include "server/PacketHandler.hpp"
#include "client/PacketStream.h"

#include <logger/Logger.hpp>

namespace highp::net {
    PacketHandler::PacketHandler(std::shared_ptr<log::Logger> logger)
        : _logger(logger) {
        //
    }

    void PacketHandler::OnAccept(std::shared_ptr<Client> client) {
    }

    void PacketHandler::OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    }

    void PacketHandler::OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) {
    }

    void PacketHandler::OnDisconnect(std::shared_ptr<Client> client) {
    }

    PacketHandler::ParseResult PacketHandler::ParsePacket(std::span<const char> data) {
        auto verifier = flatbuffers::Verifier(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size());

        if (!protocol::VerifyPacketBuffer(verifier)) {
            _logger->Warn("[PacketHandler] failed to verify. invalid packet buffer");
            return ParseResult::Err(err::EPacketError::FbFailedVerify);
        }

        return ParseResult::Ok(protocol::GetPacket(data.data()));
    }

    PacketHandler::Res PacketHandler::DispatchPacket(
        std::shared_ptr<Client> client,
        const protocol::Packet* packet
    ) {
        const auto it = _handlers.find(packet->payload_type());
        if (it == _handlers.end()) {
            _logger->Warn("[PacketHandler] unhandled payload type: {}",
                          protocol::EnumNamePayload(packet->payload_type()));
            return Res::Err(err::EPacketError::FbPayloadTypeNotFound);
        }

        it->second(client, packet);
        return Res::Ok();
    }
} // namespace highp::net
