#include "PacketSender.h"

#include "Client.h"

#include "PacketFactory.h"

void sendJoinRoom(Client* client, std::string_view nickname) {
    const auto fbb = highp::protocol::makeJoinRoomRequest(nickname);
    client->Send(fbb);
}

void sendLeave(Client* client) {
    const auto fbb = highp::protocol::makeLeaveRoomRequest();
    client->Send(fbb);
}

void sendMessage(Client* client, std::string_view message) {
    const auto fbb = highp::protocol::makeSendMessageRequest(0, message);
    client->Send(fbb);
}
