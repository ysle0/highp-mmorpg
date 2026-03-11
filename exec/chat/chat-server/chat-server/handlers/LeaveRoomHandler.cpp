#include "LeaveRoomHandler.h"
#include "../SelfHandlerRegistry.h"
#include <utility>

LeaveRoomHandler::LeaveRoomHandler(std::shared_ptr<log::Logger> logger)
    : _logger(std::move(logger)) {
}

void LeaveRoomHandler::Handle(
    std::shared_ptr<net::Client> client,
    const protocol::messages::LeaveRoomRequest* payload
) {
    _logger->Info("[LeaveRoomHandler] socket #{}, room_id={}", client->socket, payload->room_id());

    // TODO: RoomManager를 통해 방에서 유저 제거
    // TODO: LeftRoomResponse 응답
    // TODO: UserLeftBroadcast 브로드캐스트
}

SELF_REGISTER_PACKET_HANDLER(LeaveRoomHandler, true);
