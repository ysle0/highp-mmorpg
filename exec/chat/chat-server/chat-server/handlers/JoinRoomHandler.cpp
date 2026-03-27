#include "JoinRoomHandler.h"
#include <utility>

#include "PacketFactory.h"
#include "../SelfHandlerRegistry.h"

JoinRoomHandler::JoinRoomHandler(
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<RoomManager> roomManager,
    std::shared_ptr<UserManager> userManager
) : _logger(std::move(logger)),
    _roomManager(std::move(roomManager)),
    _userManager(std::move(userManager)) {
}

void JoinRoomHandler::Handle(
    std::shared_ptr<highp::net::Client> client,
    const highp::protocol::messages::JoinRoomRequest* payload
) {
    _logger->Info("[JoinRoomHandler] socket #{}", client->socket);

    const std::shared_ptr<Room> room = _roomManager->GetAvailableRoom();
    const std::shared_ptr<User> newUser = _userManager->CreateUser(
        client,
        payload->username()->c_str(),
        room->GetId());

    room->Join(newUser);

    const auto resp = highp::protocol::MakeJoinedRoomResponse();
    newUser->Send(resp);
}

// NOTE(2026-03-27): .cpp 가 static library 로 묶이면 해당 translation unit 의 심볼을 
// linker 가 참조하지 않을 시에 제거됨 -> static initializer 실행 안 됨 ->
// 핸들러 등록 안됨 -> 런타임에 패킷 드롭.
static bool registered = registerSelf<
    JoinRoomHandler,
    highp::protocol::messages::JoinRoomRequest
>(true);
