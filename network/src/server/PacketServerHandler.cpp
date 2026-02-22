#include "pch.h"
#include "server/PacketServerHandler.h"

#include <logger/Logger.hpp>

namespace highp::net {
    PacketServerHandler::PacketServerHandler(std::shared_ptr<log::Logger> logger)
        : _logger(logger) {
    }

    void PacketServerHandler::OnAccept(std::shared_ptr<Client> client) {
    }

    void PacketServerHandler::OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    }

    void PacketServerHandler::OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) {
    }

    void PacketServerHandler::OnDisconnect(std::shared_ptr<Client> client) {
    }

    void PacketServerHandler::MapPackets(std::shared_ptr<Client> client, std::span<const char> data) {
        std::string_view dataSv{data.data(), data.size()};
        _logger->Info("[PacketServerHandler::MapPackets] data: {}, bytes: {}",
                      dataSv, data.size());

        // const flatbuffers::uoffset_t bufLen = _builder.GetSize();
        // const auto packet = std::make_unique<highp::protocol::Packet>(
        // 	highp::protocol::GetPacket(
        // 		_builder.GetBufferPointer())
        // );
        //
        // const auto& packetHandler = _packetMapper.at(packet->type());
        // switch (packet->type()) {
        // 	case highp::protocol::MessageType::CS_Login:
        // 	if (auto* req = packet->payload_as_messages_LoginRequest()) {
        // 		packetHandler(std::make_unique<flatbuffers::Table>(req), client);
        // 	}
        // 	break;
        //
        // 	case highp::protocol::MessageType::CS_Logout:
        // }
    }
}
