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

// NOTE(2026-03-27): .cpp 가 static library 로 묶이면 해당 translation unit 의 심볼을 
// linker 가 참조하지 않을 시에 제거됨 -> static initializer 실행 안 됨 ->
// 핸들러 등록 안됨 -> 런타임에 패킷 드롭.
static bool registered = registerSelf<
    JoinRoomHandler,
    highp::protocol::messages::JoinRoomRequest
>(true);
