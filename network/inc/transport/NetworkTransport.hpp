#pragma once
#include "platform.h"

namespace highp::net {
    /// <summary>
    /// 전송 프로토콜 유형 열거형.
    /// </summary>
    enum class ETransport {
        /// <summary>TCP 전송 프로토콜</summary>
        TCP,

        /// <summary>UDP 전송 프로토콜</summary>
        UDP,
    };

#ifdef _WIN32

    /// <summary>
    /// Windows 플랫폼용 네트워크 전송 프로토콜 래퍼.
    /// 전송 프로토콜 유형에 따른 소켓 생성 파라미터를 제공한다.
    /// </summary>
    class WindowsNetworkTransport {
    public:
        /// <summary>
        /// WindowsNetworkTransport 생성자.
        /// </summary>
        /// <param name="transportType">전송 프로토콜 유형 (TCP/UDP)</param>
        explicit WindowsNetworkTransport(ETransport transportType) : _transportType(transportType) {
        }

        /// <summary>
        /// 소켓 생성에 필요한 소켓 타입과 프로토콜 정보를 반환한다.
        /// </summary>
        /// <returns>소켓 타입(SOCK_STREAM/SOCK_DGRAM)과 프로토콜(IPPROTO_TCP/IPPROTO_UDP) 쌍</returns>
        [[nodiscard]] constexpr std::pair<INT, IPPROTO> GetInfos() const {
            switch (_transportType) {
            case ETransport::TCP: return {SOCK_STREAM, IPPROTO_TCP};
            case ETransport::UDP: return {SOCK_DGRAM, IPPROTO_UDP};
            }
        }

        ETransport GetTransportType() const {
            return _transportType;
        }

    private:
        /// <summary>전송 프로토콜 유형</summary>
        ETransport _transportType;
    };

#elif _LINUX

    /// <summary>
    /// Linux 플랫폼용 네트워크 전송 프로토콜 래퍼. (미구현)
    /// </summary>
    class LinuxNetworkTransport {
    public:
        explicit LinuxNetworkTransport(ETransport transportType) : _transportType(transportType) {
        }

        [[nodiscard]] constexpr std::pair<INT, IPPROTO> GetInfos() const {
            switch (_transportType) {
            case ETransport::TCP: return {};
            case ETransport::UDP: return {};
            }
        }

    private:
        ETransport _transportType;
    };
#endif

#ifdef _WIN32
    /// <summary>현재 플랫폼의 네트워크 전송 타입 별칭 (Windows)</summary>
    using NetworkTransport = WindowsNetworkTransport;
#elif _LINUX
    /// <summary>현재 플랫폼의 네트워크 전송 타입 별칭 (Linux)</summary>
    using NetworkTransport = LinuxNetworkTransport;
#endif
}
