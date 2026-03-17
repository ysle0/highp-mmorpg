#include "PacketSender.h"
#include "Client.h"
#include <flatbuf/gen/packet_generated.h>

using namespace highp::protocol;
using namespace highp::protocol::messages;

void sendJoinRoom(Client* client, std::string_view nickname) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto name = builder.CreateString(nickname);
    auto req = CreateJoinRoomRequest(builder, name);
    auto pkt = CreatePacket(builder, MessageType::CS_JoinRoom,
                            Payload::messages_JoinRoomRequest, req.Union());
    FinishPacketBuffer(builder, pkt);
    client->Send(builder);
}

void sendLeave(Client* client) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto req = CreateLeaveRoomRequest(builder);
    auto pkt = CreatePacket(builder, MessageType::CS_LeaveRoom,
                            Payload::messages_LeaveRoomRequest, req.Union());
    FinishPacketBuffer(builder, pkt);
    client->Send(builder);
}

void sendMessage(Client* client, std::string_view message) {
    flatbuffers::FlatBufferBuilder builder(256);
    auto msg = builder.CreateString(message);
    auto req = CreateSendMessageRequest(builder, 0, msg);
    auto pkt = CreatePacket(builder, MessageType::CS_SendMessage,
                            Payload::messages_SendMessageRequest, req.Union());
    FinishPacketBuffer(builder, pkt);
    client->Send(builder);
}
