#include "pch.h"

#include "server/PacketDispatcher.hpp"
#include "client/PacketStream.h"
#include "config/Const.h"
#include <logger/Logger.hpp>

namespace highp::net {
    PacketDispatcher::PacketDispatcher(std::shared_ptr<log::Logger> logger)
        : _logger(std::move(logger)) {
    }

    void PacketDispatcher::Receive(std::shared_ptr<Client> client, std::span<const char> data) {
        auto& frameBuf = client->FrameBuf();

        if (!frameBuf.Append(data)) {
            _logger->Warn("[PacketDispatcher] frame buffer overflow");
            client->Close(true);
            return;
        }

        // 프레임 조립 루프: 누적 버퍼에서 완전한 프레임을 반복 추출
        while (true) {
            auto buf = frameBuf.Data();

            // 길이 헤더(4바이트)가 아직 안 모임
            if (buf.size() < PacketStream::kHeaderSize)
                break;

            // payload 길이 읽기
            uint32_t payloadLen = 0;
            std::memcpy(&payloadLen, buf.data(), PacketStream::kHeaderSize);

            if (payloadLen > Const::Buffer::maxFrameSize) {
                _logger->Warn("[PacketDispatcher] payload too large: {} bytes", payloadLen);
                _logger->Warn(
                    "[PacketDispatcher] disconnecting client. malevolent client has changed payloadLen for DoS attack.");
                client->Close(true);
                return;
            }

            const size_t frameSize = PacketStream::kHeaderSize + payloadLen;

            // payload가 아직 안 모임
            if (buf.size() < frameSize)
                break;

            // payload 영역을 파싱 + 검증 후 큐에 적재
            auto payload = buf.subspan(PacketStream::kHeaderSize, payloadLen);
            auto parseResult = ParsePacket(payload);
            if (!parseResult) {
                client->Close(true);
                return;
            }

            PushCommand(client, parseResult.Data(), payload);

            // 소비한 프레임만큼 버퍼에서 제거
            frameBuf.Consume(frameSize);
        }
    }

    void PacketDispatcher::Tick() {
        _commandQueue.DrainTo(_tickBatch);

        for (auto& cmd : _tickBatch) {
            const auto* packet = protocol::GetPacket(cmd.data.data());
            if (packet == nullptr) {
                _logger->Warn("[PacketDispatcher::Tick] failed to get packet from command");
                continue;
            }

            if (!DispatchPacket(cmd.client, packet)) {
                _logger->Warn("[PacketDispatcher::Tick] dispatch failed, closing client");
                cmd.client->Close(true);
            }
        }
    }

    void PacketDispatcher::PushCommand(
        std::shared_ptr<Client> client,
        const protocol::Packet* packet,
        std::span<const char> rawPayload
    ) {
        PacketCommand cmd{
            .client = client,
            .payloadType = packet->payload_type(),
            .data = std::vector(
                reinterpret_cast<const uint8_t*>(rawPayload.data()),
                reinterpret_cast<const uint8_t*>(rawPayload.data()) + rawPayload.size()),
        };
        _commandQueue.Push(std::move(cmd));
    }

    PacketDispatcher::Res PacketDispatcher::DispatchPacket(
        std::shared_ptr<Client> client,
        const protocol::Packet* packet
    ) {
        const auto it = _handlers.find(packet->payload_type());
        if (it == _handlers.end()) {
            _logger->Warn("[PacketDispatcher] unhandled payload type: {}",
                          protocol::EnumNamePayload(packet->payload_type()));
            return Res::Err(err::EPacketError::FbPayloadTypeNotFound);
        }

        it->second(client, packet);
        return Res::Ok();
    }

    PacketDispatcher::ParseResult PacketDispatcher::ParsePacket(std::span<const char> data) const {
        auto verifier = flatbuffers::Verifier(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size(),
            flatbuffers::VerifierTemplate<false>::Options{
                .assert = true,
            });

        if (!protocol::VerifyPacketBuffer(verifier)) {
            _logger->Warn("[PacketDispatcher] failed to verify. invalid packet buffer");
            return ParseResult::Err(err::EPacketError::FbFailedVerify);
        }

        const protocol::Packet* packet = protocol::GetPacket(data.data());
        if (packet == nullptr) {
            _logger->Warn("[PacketDispatcher] failed to get packet");
            return ParseResult::Err(err::EPacketError::FbGetPacket);
        }

        return ParseResult::Ok(packet);
    }
} // namespace highp::net
