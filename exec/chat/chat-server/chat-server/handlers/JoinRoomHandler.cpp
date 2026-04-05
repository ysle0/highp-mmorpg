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
    const std::shared_ptr<highp::net::Client>& client,
    const highp::protocol::messages::JoinRoomRequest* payload
) {
    _logger->Info("[JoinRoomHandler] socket #{}", client->socket);

    const std::shared_ptr<Room> room = _roomManager->GetOrCreateAvailableRoom();
    const std::shared_ptr<User> newUser = _userManager->CreateUser(
        client,
        payload->username()->c_str(),
        room->GetId());

    // JoinRoomRequest 가 dispatcher queue 에 적재 되어 있어 pending 상태일 때,
    // Client 가 Disconnected 가 되면 logic thread 가 Handle() 할 시점에
    // CreateUser 의 결과가 nullptr 가 가능하므로 early-return 처리.
    if (!newUser) {
        _logger->Warn("[JoinRoomHandler] failed to create user for socket #{}; session may already be gone",
                      client->socket);
        return;
    }

    room->Join(newUser);

    const flatbuffers::FlatBufferBuilder resp = highp::protocol::makeJoinedRoomResponse();
    newUser->Send(resp);
}

// NOTE(2026-03-27): .cpp 가 static library 로 묶이면 해당 translation unit 의 심볼을 
// linker 가 참조하지 않을 시에 제거됨 -> static initializer 실행 안 됨 ->
// 핸들러 등록 안됨 -> 런타임에 패킷 드롭.
static bool registered = registerSelf<
    JoinRoomHandler,
    highp::protocol::messages::JoinRoomRequest
>(true);
