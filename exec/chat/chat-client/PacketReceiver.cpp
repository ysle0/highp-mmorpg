#include "PacketReceiver.h"
#include <flatbuf/gen/packet_generated.h>

using namespace highp;
using namespace highp::protocol;
using namespace highp::protocol::messages;

void OnPacketReceived(std::shared_ptr<log::Logger> logger, const Packet* packet) {
    switch (packet->type()) {
    case MessageType::SC_JoinedRoom: {
        auto* resp = packet->payload_as_messages_JoinedRoomResponse();
        if (!resp) break;
        if (resp->ok()) {
            logger->Info("[JoinedRoom] ok=true");
            if (auto* rw = resp->room(); rw && rw->room()) {
                logger->Info("  room: id={}, name={}", rw->room()->id(),
                             rw->room()->name() ? rw->room()->name()->c_str() : "");
            }
        }
        else {
            auto* err = resp->error();
            logger->Warn("[JoinedRoom] ok=false, error={}",
                         err && err->message() ? err->message()->c_str() : "unknown");
        }
        break;
    }
    case MessageType::SC_LeftRoom: {
        auto* resp = packet->payload_as_messages_LeftRoomResponse();
        if (!resp) break;
        if (resp->ok()) {
            logger->Info("[LeftRoom] ok=true");
        }
        else {
            auto* err = resp->error();
            logger->Warn("[LeftRoom] ok=false, error={}",
                         err && err->message() ? err->message()->c_str() : "unknown");
        }
        break;
    }
    case MessageType::SC_UserJoined: {
        auto* bc = packet->payload_as_messages_UserJoinedBroadcast();
        if (!bc) break;
        auto* user = bc->user();
        logger->Info("[UserJoined] room={}, user={}",
                     bc->room_id(),
                     user && user->username() ? user->username()->c_str() : "?");
        break;
    }
    case MessageType::SC_UserLeft: {
        auto* bc = packet->payload_as_messages_UserLeftBroadcast();
        if (!bc) break;
        logger->Info("[UserLeft] room={}, user={}",
                     bc->room_id(),
                     bc->username() ? bc->username()->c_str() : "?");
        break;
    }
    case MessageType::SC_ChatMessage: {
        auto* bc = packet->payload_as_messages_ChatMessageBroadcast();
        if (!bc) break;
        auto* sender = bc->sender();
        logger->Info("[Chat] room={}, {}: {}",
                     bc->room_id(),
                     sender && sender->username() ? sender->username()->c_str() : "?",
                     bc->message() ? bc->message()->c_str() : "");
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
    default:
        logger->Warn("[Unknown] type={}", EnumNameMessageType(packet->type()));
        break;
    }
}
