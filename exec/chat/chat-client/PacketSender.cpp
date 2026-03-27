#include "PacketSender.h"

#include "Client.h"

#include "PacketFactory.h"

void sendJoinRoom(Client* client, std::string_view nickname) {
    const auto fbb = highp::protocol::MakeJoinRoomRequest(nickname);
    client->Send(fbb);
}

void sendLeave(Client* client) {
    const auto fbb = highp::protocol::MakeLeaveRoomRequest();
    client->Send(fbb);
}

void sendMessage(Client* client, std::string_view message) {
    const auto fbb = highp::protocol::MakeSendMessageRequest(0, message);
    client->Send(fbb);
}
