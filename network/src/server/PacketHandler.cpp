#include "pch.h"
#include "server/PacketHandler.hpp"
#include "client/PacketStream.h"
#include "config/Const.h"

#include <logger/Logger.hpp>

namespace highp::net {
    PacketHandler::PacketHandler(std::shared_ptr<log::Logger> logger)
        : _logger(logger) {
        //
    }

    void PacketHandler::OnAccept(std::shared_ptr<Client> client) {
    }

    void PacketHandler::OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
        auto& frameBuf = client->FrameBuf();

        if (!frameBuf.Append(data)) {
            _logger->Warn("[PacketServerHandler] frame buffer overflow");
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
                _logger->Warn("[PacketHandler] payload too large: {} bytes", payloadLen);
                _logger->Warn("[PacketHandler] disconnecting client. malevolent client has changed payloadLen for DoS attack.");
                _logger->Warn("[PacketHandler] you should consider further security measure such as IP based client ban, rate limiting for the same client.");
                client->Close(true);
                return;
            }

            const size_t frameSize = PacketStream::kHeaderSize + payloadLen;

            // payload가 아직 안 모임
            if (buf.size() < frameSize)
                break;

            // payload 영역을 파싱 후 디스패치
            auto payload = buf.subspan(PacketStream::kHeaderSize, payloadLen);
            auto parseResult = ParsePacket(payload);
            if (!parseResult) {
                client->Close(true);
                return;
            }
            if (!DispatchPacket(client, parseResult.Data())) {
                client->Close(true);
                return;
            }

            // 소비한 프레임만큼 버퍼에서 제거
            frameBuf.Consume(frameSize);
        }
    }

    void PacketHandler::OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) {
    }

    void PacketHandler::OnDisconnect(std::shared_ptr<Client> client) {
    }
} // namespace highp::net
