#include "pch.h"
#include "socket/AsyncSocket.h"

namespace highp::net {
    AsyncSocket::AsyncSocket(std::shared_ptr<log::Logger> logger)
        : _socketHandle{InvalidSocket}
          , _logger{logger} {
    }

    ISocket::Res AsyncSocket::Initialize() {
        return Res::Ok();
    }

    ISocket::Res AsyncSocket::CreateSocket(NetworkTransport) {
        return Res::Ok();
    }

    ISocket::Res AsyncSocket::Bind(unsigned short port) {
        return Res::Ok();
    }

    ISocket::Res AsyncSocket::Listen(int backlog) {
        return Res::Ok();
    }

    ISocket::Res AsyncSocket::Cleanup() {
        return Res::Ok();
    }
}
