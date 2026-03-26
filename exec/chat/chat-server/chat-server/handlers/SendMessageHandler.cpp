#include "SendMessageHandler.h"
#include "../SelfHandlerRegistry.h"
#include <utility>

SendMessageHandler::SendMessageHandler(std::shared_ptr<log::Logger> logger)
    : _logger(std::move(logger)) {
}

void SendMessageHandler::Handle(
    std::shared_ptr<net::Client> client,
    const protocol::messages::SendMessageRequest* payload
) {
    _logger->Info("[SendMessageHandler] socket #{}, room_id={}, message={}",
        client->socket, payload->room_id(), payload->message()->c_str());

    // TODO: RoomManager를 통해 해당 방의 모든 유저에게 ChatMessageBroadcast 전송
}

SELF_REGISTER_PACKET_HANDLER(SendMessageHandler, true);
