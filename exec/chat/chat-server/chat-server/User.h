#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <flatbuffers/flatbuffers.h>

#include "Session.h"
#include "client/windows/Client.h"

class User {
public:
    explicit User(
        std::shared_ptr<Session> session,
        uint64_t userId,
        std::string_view username,
        uint64_t roomId);

    void Send(const flatbuffers::FlatBufferBuilder& builder) const;

public:
    [[nodiscard]] bool IsSameClient(const std::shared_ptr<highp::net::Client>& client) const;
    [[nodiscard]] bool IsSameUser(const User& user) const;

    [[nodiscard]] uint64_t GetId() const { return _id; }
    [[nodiscard]] std::string_view GetUsername() const { return _userName; }
    [[nodiscard]] uint64_t GetRoomId() const { return _roomId; }

private:
    std::shared_ptr<Session> _session;
    uint64_t _id{0};
    std::string _userName;
    uint64_t _roomId{0};
};
