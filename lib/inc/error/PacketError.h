#pragma once

namespace highp::err {
    enum class EPacketError {
        Unknown = 0,

        FbFailedVerify,
        FbFailedSerialize,
        FbGetPacket,
        FbFailedDeserialize,
        FbPayloadTypeNotFound,
    };

    constexpr std::string_view toString(EPacketError err) noexcept {
        switch (err) {
        case EPacketError::FbFailedVerify: return "PacketFlatbufferFailedVerify";
        case EPacketError::FbFailedSerialize: return "PacketFlatbufferFailedSerialize";
        case EPacketError::FbGetPacket: return "PacketFlatbufferGetPacket";
        case EPacketError::FbFailedDeserialize: return "PacketFlatbufferFailedDeserialize";
        case EPacketError::FbPayloadTypeNotFound: return "PacketFlatbufferPayloadTypeNotFound";

        case EPacketError::Unknown:
        default: return "Unknown";
        }
    }
} // namespace highp::err
