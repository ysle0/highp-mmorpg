#pragma once
#include "Session.h"
#include "client/windows/Client.h"
#include "handlers/ChatMessageHandler.h"

class User {
public:
    explicit User(std::shared_ptr<Session> session);

    void Send(const flatbuffers::FlatBufferBuilder& builder) const;

    [[nodiscard]] std::string_view GetUserName() const { return std::string_view(_userName); }
    [[nodiscard]] std::uint32_t GetId() const { return _id; }

private:
    std::shared_ptr<Session> _session;
    std::string _userName;
    std::uint32_t _id;
};
