#include "pch.h"
#include <error/NetworkError.h>
#include <error/Errors.hpp>
#include "socket/WindowsAsyncSocket.h"

namespace highp::net::internal {
    WindowsAsyncSocket::WindowsAsyncSocket(
        std::shared_ptr<log::Logger> logger
    ) : AsyncSocket{logger->WithPrefix("[WindowsAsyncSocket] ")} {
    }

    WindowsAsyncSocket::~WindowsAsyncSocket() {
        Cleanup();
    }

    WindowsAsyncSocket::Res WindowsAsyncSocket::Initialize() {
        if (_wsaStarted) {
            return Res::Ok();
        }

        WSADATA wsaData;
        if (int res = WSAStartup(MAKEWORD(2, 2), &wsaData); res != 0) {
            return err::LogErrorWSAWithResult<err::ENetworkError::WsaStartupFailed>(_logger);
        }

        _wsaStarted = true;
        return Res::Ok();
    }

    WindowsAsyncSocket::Res WindowsAsyncSocket::CreateSocket(NetworkTransport transport) {
        auto [socketType, protocol] = transport.GetInfos();
        _socketHandle = WSASocket(
            AF_INET, /* Address Family (AF_INET, AF_INET6) */
            socketType, /* Socket Type (SOCK_STREAM, SOCK_DGRAM) */
            protocol, /* Protocol (IPPROTO_TCP, IPPROTOUDP) */
            nullptr, /* Protocol Information Structure (NULL in most case) */
            0, /* Socket Group */
            WSA_FLAG_OVERLAPPED /* Socket Attribute Flags */
        );
        if (_socketHandle == INVALID_SOCKET) {
            return err::LogErrorByWSAStartupResult(_logger);
        }
        return Res::Ok();
    }

    WindowsAsyncSocket::Res WindowsAsyncSocket::Bind(unsigned short port) {
        if (!IsValidPort(port)) {
            // TODO: Error Log
            return Res::Ok();
        }

        _sockaddr = {
            .sin_family = AF_INET,
            .sin_port = htons(port)
        };
        _sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(_socketHandle, (SOCKADDR*)&_sockaddr, sizeof(SOCKADDR_IN)) != 0) {
            return err::LogErrorWSAWithResult<err::ENetworkError::SocketBindFailed>(_logger);
        }

        return Res::Ok();
    }

    WindowsAsyncSocket::Res WindowsAsyncSocket::Listen(int backlog) {
        if (_socketHandle == INVALID_SOCKET) {
            return err::LogErrorWSAWithResult<err::ENetworkError::SocketInvalid>(_logger);
        }

        if (listen(_socketHandle, backlog) != 0) {
            return err::LogErrorWSAWithResult<err::ENetworkError::SocketListenFailed>(_logger);
        }

        return Res::Ok();
    }

    WindowsAsyncSocket::Res WindowsAsyncSocket::Cleanup() {
        if (_socketHandle != InvalidSocket) {
            closesocket(_socketHandle);
            _socketHandle = InvalidSocket;
        }

        if (_wsaStarted) {
            WSACleanup();
            _wsaStarted = false;
        }

        return Res::Ok();
    }
}
