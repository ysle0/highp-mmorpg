#include "LeaveRoomHandler.h"
#include "../SelfHandlerRegistry.h"
#include <utility>

LeaveRoomHandler::LeaveRoomHandler(std::shared_ptr<highp::log::Logger> logger)
    : _logger(std::move(logger)) {
}

void LeaveRoomHandler::Handle(
    std::shared_ptr<highp::net::Client> client,
    const highp::protocol::messages::LeaveRoomRequest* payload
) {
    _logger->Info("[LeaveRoomHandler] socket #{}, room_id={}", client->socket, payload->room_id());

    // TODO: RoomManager를 통해 방에서 유저 제거
    // TODO: LeftRoomResponse 응답
    // TODO: UserLeftBroadcast 브로드캐스트
}

static bool registered = registerSelf<
    LeaveRoomHandler,
    highp::protocol::messages::LeaveRoomRequest
>(true);
