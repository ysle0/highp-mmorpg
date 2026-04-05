#include "LeaveRoomHandler.h"
#include "../SelfHandlerRegistry.h"
#include <utility>

#include "PacketFactory.h"

LeaveRoomHandler::LeaveRoomHandler(
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<RoomManager> roomManager,
    std::shared_ptr<UserManager> userManager
) : _logger(std::move(logger)),
    _roomManager(std::move(roomManager)),
    _userManager(std::move(userManager)) {
}

void LeaveRoomHandler::Handle(
    const std::shared_ptr<highp::net::Client>& client,
    const highp::protocol::messages::LeaveRoomRequest* payload
) {
    _logger->Info("[LeaveRoomHandler] socket #{}, room_id={}",
                  client->socket,
                  payload->room_id());

    const User* user = _userManager->GetUserByClient(client);
    if (!user) {
        _logger->Warn("[LeaveRoomHandler] user not found");
        return;
    }

    Room* room = _roomManager->GetRoom(user->GetRoomId());
    room->Leave(user->GetId());

    _logger->Info("[LeaveRoomHandler] user left room");

    const flatbuffers::FlatBufferBuilder resp = highp::protocol::makeLeftRoomResponse();
    user->Send(resp);

    _userManager->RemoveUserByUserId(user->GetId());
}

// NOTE(2026-03-27): .cpp 가 static library 로 묶이면 해당 translation unit 의 심볼을 
// linker 가 참조하지 않을 시에 제거됨 -> static initializer 실행 안 됨 ->
// 핸들러 등록 안됨 -> 런타임에 패킷 드롭.
static bool registered = registerSelf<
    LeaveRoomHandler,
    highp::protocol::messages::LeaveRoomRequest
>(true);
