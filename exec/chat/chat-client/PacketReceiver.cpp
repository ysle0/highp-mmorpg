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
        const JoinedRoomResponse* resp = packet->payload_as_messages_JoinedRoomResponse();
        if (const Error* err = resp->error(); err != nullptr) {
            logger->Warn("[JoinedRoom] err={}", err->message()->c_str());
            break;
        }

        logger->Info("[JoinedRoom] ok");
        break;
    }

    case MessageType::SC_LeftRoom: {
        const LeftRoomResponse* resp = packet->payload_as_messages_LeftRoomResponse();
        if (const Error* err = resp->error(); err != nullptr) {
            logger->Warn("[LeftRoom] err={}", err->message()->c_str());
            break;
        }

        logger->Info("[LeftRoom] ok");
        break;
    }

    case MessageType::B_UserJoined: {
        const UserJoinedBroadcast* bc = packet->payload_as_messages_UserJoinedBroadcast();
        const User* user = bc->user();
        const uint32_t roomId = bc->room_id();
        logger->Info("[UserJoined] user={} roomId={}",
                     user->username()->c_str(),
                     roomId);
        break;
    }

    case MessageType::B_UserLeft: {
        const UserLeftBroadcast* bc = packet->payload_as_messages_UserLeftBroadcast();
        logger->Info("[UserLeft] user={}", bc->username()->c_str());
        break;
    }

    case MessageType::B_ChatMessage: {
        const ChatMessageBroadcast* bc = packet->payload_as_messages_ChatMessageBroadcast();
        logger->Info("[Chat] {}[{}]: {}",
                     bc->sender_id(),
                     bc->username()->c_str(),
                     bc->message()->c_str());
        break;
    }

    case MessageType::SC_Error: {
        const ErrorResponse* err = packet->payload_as_ErrorResponse();
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
