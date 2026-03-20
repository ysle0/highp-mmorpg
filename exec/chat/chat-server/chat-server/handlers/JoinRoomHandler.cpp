#include "JoinRoomHandler.h"
#include <utility>

#include "../SelfHandlerRegistry.h"

JoinRoomHandler::JoinRoomHandler(std::shared_ptr<highp::log::Logger> logger)
    : _logger(std::move(logger)) {
}

void JoinRoomHandler::Handle(
    std::shared_ptr<highp::net::Client> client,
    const highp::protocol::messages::JoinRoomRequest* payload
) {
    _logger->Info("[JoinRoomHandler] socket #{}", client->socket);
}

// SELF_REGISTER_PACKET_HANDLER(JoinRoomHandler, true);
static bool registered = registerSelf<
    JoinRoomHandler,
    highp::protocol::messages::JoinRoomRequest
>(true);
