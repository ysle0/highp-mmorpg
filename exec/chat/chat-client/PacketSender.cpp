#include "PacketSender.h"
#include "Client.h"
#include <flatbuf/gen/packet_generated.h>

using namespace highp::protocol;
using namespace highp::protocol::messages;

void SendJoinRoom(Client& client, uint32_t roomId) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto req = CreateJoinRoomRequest(builder, roomId);
    auto pkt = CreatePacket(builder, MessageType::CS_JoinRoom,
                            Payload::messages_JoinRoomRequest, req.Union());
    FinishPacketBuffer(builder, pkt);
    client.Send(builder);
}

void SendLeaveRoom(Client& client, uint32_t roomId) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto req = CreateLeaveRoomRequest(builder, roomId);
    auto pkt = CreatePacket(builder, MessageType::CS_LeaveRoom,
                            Payload::messages_LeaveRoomRequest, req.Union());
    FinishPacketBuffer(builder, pkt);
    client.Send(builder);
}

void SendMessage(Client& client, uint32_t roomId, const std::string& message) {
    flatbuffers::FlatBufferBuilder builder(256);
    auto msg = builder.CreateString(message);
    auto req = CreateSendMessageRequest(builder, roomId, msg);
    auto pkt = CreatePacket(builder, MessageType::CS_SendMessage,
                            Payload::messages_SendMessageRequest, req.Union());
    FinishPacketBuffer(builder, pkt);
    client.Send(builder);
}
