#include "ChatMessageHandler.h"
#include "../SelfHandlerRegistry.h"
#include <utility>

ChatMessageHandler::ChatMessageHandler(std::shared_ptr<highp::log::Logger> logger)
    : _logger(std::move(logger)) {
}

void ChatMessageHandler::Handle(
    std::shared_ptr<highp::net::Client> client,
    const highp::protocol::messages::SendMessageRequest* payload
) {
    _logger->Info("[ChatMessageHandler] socket #{}, room_id={}, message={}",
                  client->socket, payload->room_id(), payload->message()->c_str());

    // TODO: RoomManager를 통해 해당 방의 모든 유저에게 ChatMessageBroadcast 전송
}

// NOTE(2026-03-27): .cpp 가 static library 로 묶이면 해당 translation unit 의 심볼을 
// linker 가 참조하지 않을 시에 제거됨 -> static initializer 실행 안 됨 ->
// 핸들러 등록 안됨 -> 런타임에 패킷 드롭.
static bool registered = registerSelf<
    ChatMessageHandler,
    highp::protocol::messages::SendMessageRequest
>(true);
