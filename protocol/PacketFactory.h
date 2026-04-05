#pragma once

#include <cstdint>
#include <string_view>

#include <flatbuf/gen/packet_generated.h>

#include "time/Time.h"

namespace highp::protocol::detail {
    inline flatbuffers::Offset<flatbuffers::String> createStringOffset(
        flatbuffers::FlatBufferBuilder& builder,
        std::string_view value
    ) {
        return builder.CreateString(value.empty() ? "" : value.data(), value.size());
    }

    // Wrap a typed payload into the root Packet using generated union traits.
    template <typename TPayload>
    inline void finishTypedPacket(
        flatbuffers::FlatBufferBuilder& builder,
        MessageType type,
        flatbuffers::Offset<TPayload> payload,
        std::uint32_t sequence = 0
    ) {
        static_assert(
            PayloadTraits<TPayload>::enum_value != Payload::NONE,
            "TPayload must map to a protocol::Payload enum."
        );

        const auto packet = CreatePacket(
            builder,
            type,
            PayloadTraits<TPayload>::enum_value,
            payload.Union(),
            sequence);
        FinishPacketBuffer(builder, packet);
    }
} // namespace detail

namespace highp::protocol {
    inline Common::Timestamp now() {
        return Common::Timestamp(
            time::Time::NowInSec(),
            time::Time::NowInNs()
        );
    }


    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeJoinRoomRequest(
        std::string_view nickname,
        std::uint32_t sequence = 0
    ) {
        flatbuffers::FlatBufferBuilder builder(128);
        const auto request = messages::CreateJoinRoomRequest(
            builder,
            detail::createStringOffset(builder, nickname));
        detail::finishTypedPacket(builder, MessageType::CS_JoinRoom, request, sequence);
        return builder;
    }

    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeLeaveRoomRequest(
        std::uint32_t roomId = 0,
        std::uint32_t sequence = 0
    ) {
        flatbuffers::FlatBufferBuilder builder(128);
        const auto request = messages::CreateLeaveRoomRequest(builder, roomId);
        detail::finishTypedPacket(builder, MessageType::CS_LeaveRoom, request, sequence);
        return builder;
    }

    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeSendMessageRequest(
        std::uint32_t roomId,
        std::string_view username,
        std::string_view message,
        std::uint32_t sequence = 0
    ) {
        flatbuffers::FlatBufferBuilder builder(256);
        const auto request = messages::CreateSendMessageRequest(
            builder,
            roomId,
            detail::createStringOffset(builder, username),
            detail::createStringOffset(builder, message));
        detail::finishTypedPacket(builder, MessageType::CS_SendMessage, request, sequence);
        return builder;
    }

    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeUserJoinedBroadcast(
        uint32_t roomId,
        std::uint64_t userId,
        std::string_view userName,
        UserStatus status = UserStatus::Online,
        std::uint32_t sequence = 0
    ) {
        flatbuffers::FlatBufferBuilder builder(128);
        const auto user = CreateUser(
            builder,
            userId,
            detail::createStringOffset(builder, userName),
            status);
        const auto broadcast = messages::CreateUserJoinedBroadcast(builder, roomId, user);
        detail::finishTypedPacket(builder, MessageType::B_UserJoined, broadcast, sequence);
        return builder;
    }

    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeUserLeftBroadcast(
        std::uint64_t userId,
        std::string_view userName,
        std::uint32_t sequence = 0
    ) {
        flatbuffers::FlatBufferBuilder builder(128);
        const auto broadcast = messages::CreateUserLeftBroadcast(
            builder,
            userId,
            detail::createStringOffset(builder, userName));
        detail::finishTypedPacket(builder, MessageType::B_UserLeft, broadcast, sequence);
        return builder;
    }

    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeChatMessageBroadcast(
        std::uint32_t senderId,
        std::string_view username,
        std::string_view message,
        const Common::Timestamp& timestamp,
        std::uint32_t sequence = 0
    ) {
        flatbuffers::FlatBufferBuilder builder(256);
        const auto broadcast = messages::CreateChatMessageBroadcast(
            builder,
            senderId,
            detail::createStringOffset(builder, username),
            detail::createStringOffset(builder, message),
            &timestamp);
        detail::finishTypedPacket(builder, MessageType::B_ChatMessage, broadcast, sequence);
        return builder;
    }

    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeJoinedRoomResponse() {
        flatbuffers::FlatBufferBuilder builder(128);
        const auto resp = messages::CreateJoinedRoomResponse(builder);
        detail::finishTypedPacket(builder, MessageType::SC_JoinedRoom, resp);
        return builder;
    }

    [[nodiscard]] inline flatbuffers::FlatBufferBuilder makeLeftRoomResponse() {
        flatbuffers::FlatBufferBuilder builder(128);
        const auto resp = messages::CreateLeftRoomResponse(builder);
        detail::finishTypedPacket(builder, MessageType::SC_LeftRoom, resp);
        return builder;
    }
} // namespace highp::protocol
