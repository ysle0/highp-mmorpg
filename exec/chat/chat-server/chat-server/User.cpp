#include "User.h"

#include "time/Time.h"

User::User(std::shared_ptr<Session> session) : _session(std::move(session)), _id(0) {
}

void User::Send(const flatbuffers::FlatBufferBuilder& builder) const {
    _session->Send(builder);
}

bool User::IsSameUser(const std::shared_ptr<highp::net::Client>& client) const {
    return _session->IsSameSession(client);
}
