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

// NOTE(2026-03-27): .cpp 가 static library 로 묶이면 해당 translation unit 의 심볼을 
// linker 가 참조하지 않을 시에 제거됨 -> static initializer 실행 안 됨 ->
// 핸들러 등록 안됨 -> 런타임에 패킷 드롭.
static bool registered = registerSelf<
    LeaveRoomHandler,
    highp::protocol::messages::LeaveRoomRequest
>(true);
