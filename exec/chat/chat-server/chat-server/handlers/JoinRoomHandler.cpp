#include "JoinRoomHandler.h"
#include <utility>

using namespace highp;

JoinRoomHandler::JoinRoomHandler(std::shared_ptr<log::Logger> logger)
    : _logger(std::move(logger)) {
}

void JoinRoomHandler::Handle(
    std::shared_ptr<net::Client> client,
    const protocol::messages::JoinRoomRequest* payload
) {
    _logger->Info("[JoinRoomHandler] socket #{}", client->socket);
}
