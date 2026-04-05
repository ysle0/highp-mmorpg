#include "PacketReceiver.h"
#include <flatbuf/gen/packet_generated.h>

using namespace highp;
using namespace highp::protocol;
using namespace highp::protocol::messages;

void onPacketReceived(const std::shared_ptr<log::Logger>& logger, const Packet* packet) {
    if (packet == nullptr) {
        logger->Warn("[Invalid] packet is null");
        return;
    }

    switch (packet->type()) {
    case MessageType::SC_JoinedRoom: {
        auto* resp = packet->payload_as_messages_JoinedRoomResponse();
        if (auto* err = resp->error(); err != nullptr) {
            logger->Warn("[JoinedRoom] err={}", err->message()->c_str());
            break;
        }

        logger->Info("[JoinedRoom] ok");
        break;
    }

    case MessageType::SC_LeftRoom: {
        auto* resp = packet->payload_as_messages_LeftRoomResponse();
        if (auto* err = resp->error(); err != nullptr) {
            logger->Warn("[LeftRoom] err={}", err->message()->c_str());
            break;
        }

        logger->Info("[LeftRoom] ok");
        break;
    }

    case MessageType::B_UserJoined: {
        auto* bc = packet->payload_as_messages_UserJoinedBroadcast();
        auto* user = bc->user();
        logger->Info("[UserJoined] user={}", user->username()->c_str());
        break;
    }

    case MessageType::B_UserLeft: {
        auto* bc = packet->payload_as_messages_UserLeftBroadcast();
        logger->Info("[UserLeft] user={}", bc->username()->c_str());
        break;
    }

    case MessageType::B_ChatMessage: {
        auto* bc = packet->payload_as_messages_ChatMessageBroadcast();
        uint32_t senderId = bc->sender_id();
        logger->Info("[Chat] {}: {}",
                     senderId,
                     bc->message()->c_str());
        break;
    }

    case MessageType::SC_Error: {
        auto* err = packet->payload_as_ErrorResponse();
        if (!err) break;
        logger->Error("[ServerError] code={}, message={}",
                      EnumNameErrorCode(err->error_code()),
                      err->message() ? err->message()->c_str() : "");
        break;
    }

    case MessageType::None:
    case MessageType::CS_JoinRoom:
    case MessageType::CS_LeaveRoom:
    case MessageType::CS_SendMessage:
        logger->Error("[Invalid] type={}", EnumNameMessageType(packet->type()));
        break;

    default:
        logger->Warn("[Unknown] type={}", EnumNameMessageType(packet->type()));
        break;
    }
}
