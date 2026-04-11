#include "PacketSender.h"

#include "Client.h"

#include "PacketFactory.h"

void sendJoinRoom(Client* client, std::string_view nickname) {
    const uint32_t sequence = client->NextSequence();
    const auto fbb = highp::protocol::makeJoinRoomRequest(nickname, sequence);
    client->Send(fbb, {
        .sequence = sequence,
        .responseType = highp::protocol::MessageType::SC_JoinedRoom,
    });
}

void sendLeave(Client* client) {
    const uint32_t sequence = client->NextSequence();
    const auto fbb = highp::protocol::makeLeaveRoomRequest(0, sequence);
    client->Send(fbb, {
        .sequence = sequence,
        .responseType = highp::protocol::MessageType::SC_LeftRoom,
    });
}

void sendMessage(Client* client, std::string_view username, std::string_view message) {
    const uint32_t sequence = client->NextSequence();
    const auto fbb = highp::protocol::makeSendMessageRequest(0, username, message, sequence);
    client->Send(fbb, {
        .sequence = sequence,
        .responseType = highp::protocol::MessageType::B_ChatMessage,
    });
}
