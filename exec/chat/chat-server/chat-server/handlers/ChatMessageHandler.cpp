#include "ChatMessageHandler.h"
#include "../SelfHandlerRegistry.h"
#include <utility>

ChatMessageHandler::ChatMessageHandler(
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<RoomManager> roomManager,
    std::shared_ptr<UserManager> userManager
) : _logger(std::move(logger)),
    _roomManager(std::move(roomManager)),
    _userManager(std::move(userManager)) {
}

void ChatMessageHandler::Handle(
    std::shared_ptr<highp::net::Client> client,
    const highp::protocol::messages::SendMessageRequest* payload
) {
    _logger->Info("[ChatMessageHandler] socket #{}, room_id={}, message={}",
                  client->socket, payload->room_id(), payload->message()->c_str());

    const auto user = _userManager->GetUserByClient(client);
    if (!user) {
        _logger->Warn("[ChatMessageHandler] user not found");
        return;
    }

    if (Room* r = _roomManager->GetRoom(user->GetRoomId())) {
        std::string_view msg{payload->message()->str()};
        _logger->Debug("[ChatMessageHandler] broadcasting message: {}", msg);

        r->BroadcastChatMessage(user->GetId(), msg);
    }
    else {
        _logger->Warn("[ChatMessageHandler] room not found: {}", user->GetRoomId());
    }
}

// NOTE(2026-03-27): .cpp 가 static library 로 묶이면 해당 translation unit 의 심볼을 
// linker 가 참조하지 않을 시에 제거됨 -> static initializer 실행 안 됨 ->
// 핸들러 등록 안됨 -> 런타임에 패킷 드롭.
static bool registered = registerSelf<
    ChatMessageHandler,
    highp::protocol::messages::SendMessageRequest
>(true);
