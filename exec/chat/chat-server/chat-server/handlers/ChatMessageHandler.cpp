#include "ChatMessageHandler.h"
#include <utility>

using namespace highp;

ChatMessageHandler::ChatMessageHandler(std::shared_ptr<log::Logger> logger)
    : _logger(std::move(logger)) {
}

void ChatMessageHandler::Handle(
    std::shared_ptr<net::Client> client,
    const protocol::messages::ChatMessageBroadcast* payload
) {
    _logger->Info("[ChatMessageHandler] socket #{}", client->socket);

    // 1. print received message with timestamp
}
