#include "User.h"

#include "time/Time.h"

User::User(
    std::shared_ptr<Session> session,
    uint64_t id,
    std::string_view username,
    uint64_t roomId
) : _session(std::move(session)),
    _id(id),
    _userName(std::move(username)),
    _roomId(roomId) {
}

void User::Send(const flatbuffers::FlatBufferBuilder& builder) const {
    _session->Send(builder);
}

bool User::IsSameClient(const std::shared_ptr<highp::net::Client>& client) const {
    return _session->IsSameSession(client);
}

bool User::IsSameUser(const User& user) const {
    return
        _id == user._id &&
        _userName == user._userName;
}
