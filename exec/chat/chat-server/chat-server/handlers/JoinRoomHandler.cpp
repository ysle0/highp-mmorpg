#include "JoinRoomHandler.h"
#include <iostream>

#include "../SelfHandlerRegistry.h"

JoinRoomHandler::JoinRoomHandler(std::shared_ptr<log::Logger> logger)
    : _logger(logger) {
}

void JoinRoomHandler::Handle(
    std::shared_ptr<net::Client> client,
    const protocol::messages::JoinRoomRequest* payload
) {
    _logger->Info("[JoinRoomHandler] socket #{}", client->socket);
    std::cout << "JoinRoomRequest" << std::endl;
}

SELF_REGISTER_PACKET_HANDLER(JoinRoomHandler, true);