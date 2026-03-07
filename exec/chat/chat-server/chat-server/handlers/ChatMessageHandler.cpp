#include "ChatMessageHandler.h"
#include <iostream>

ChatMessageHandler::ChatMessageHandler(std::shared_ptr<log::Logger> logger)
    : _logger(logger) {
}

void ChatMessageHandler::Handle(
    std::shared_ptr<net::Client> client,
    const protocol::messages::ChatMessageBroadcast* payload
) {
    _logger->Info("[ChatMessageHandler] socket #{}", client->socket);
    std::cout << "ChatMessageBroadcast" << std::endl;
}
