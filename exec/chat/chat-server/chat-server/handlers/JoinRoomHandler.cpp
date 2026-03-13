#include "JoinRoomHandler.h"
#include <utility>

using namespace highp;

#include "../SelfHandlerRegistry.h"

JoinRoomHandler::JoinRoomHandler(std::shared_ptr<log::Logger> logger)
    : _logger(std::move(logger)) {
}

void JoinRoomHandler::Handle(
    std::shared_ptr<net::Client> client,
    const protocol::messages::JoinRoomRequest* payload
) {
    _logger->Info("[JoinRoomHandler] socket #{}", client->socket);
}

SELF_REGISTER_PACKET_HANDLER(JoinRoomHandler, true);