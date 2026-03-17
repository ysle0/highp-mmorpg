#include "ChatMessageHandler.h"
#include "../SelfHandlerRegistry.h"
#include <utility>

ChatMessageHandler::ChatMessageHandler(std::shared_ptr<highp::log::Logger> logger)
    : _logger(std::move(logger)) {
}

void ChatMessageHandler::Handle(
    std::shared_ptr<highp::net::Client> client,
    const highp::protocol::messages::SendMessageRequest* payload
) {
    _logger->Info("[ChatMessageHandler] socket #{}, room_id={}, message={}",
                  client->socket, payload->room_id(), payload->message()->c_str());

    // TODO: RoomManager를 통해 해당 방의 모든 유저에게 ChatMessageBroadcast 전송
}

static bool registered = registerSelf<
    ChatMessageHandler,
    highp::protocol::messages::SendMessageRequest
>(true);
