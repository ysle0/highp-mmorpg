#pragma once

#include <memory>
#include <span>
#include <string_view>
#include <error/NetworkError.h>
#include <functional/Result.hpp>
#include "platform.h"

namespace highp::log {
    class Logger;
}

namespace highp::net {
    class WsaSession;

    class TcpClientSocket final {
    public:
        using Res = fn::Result<void, err::ENetworkError>;
        using ResWithSize = fn::Result<size_t, err::ENetworkError>;

        TcpClientSocket(std::shared_ptr<log::Logger> logger, std::shared_ptr<WsaSession> wsaSession);
        ~TcpClientSocket() noexcept;

        TcpClientSocket(const TcpClientSocket&) = delete;
        TcpClientSocket& operator=(const TcpClientSocket&) = delete;
        TcpClientSocket(TcpClientSocket&&) = delete;
        TcpClientSocket& operator=(TcpClientSocket&&) = delete;

        Res Connect(std::string_view ipAddress, unsigned short port);
        Res SendAll(std::span<const char> data);
        ResWithSize RecvSome(std::span<char> buffer);
        Res Close();

        [[nodiscard]] bool IsConnected() const noexcept;
        [[nodiscard]] SocketHandle GetSocketHandle() const noexcept;

    private:
        std::shared_ptr<log::Logger> _logger;
        std::shared_ptr<WsaSession> _wsaSession;
        SocketHandle _socketHandle = InvalidSocket;
    };
}
