#include "pch.h"
#include "socket/SocketHelper.h"
#include "socket/WindowsAsyncSocket.h"
#include "socket/SocketOptionBuilder.h"

namespace highp::net {
    std::shared_ptr<ISocket> SocketHelper::MakeDefaultListener(
        std::shared_ptr<log::Logger> logger,
        NetworkTransport netTransport,
        NetworkCfg netCfg,
        std::shared_ptr<SocketOptionBuilder> socketOptionBuilder
    ) {
        auto s = std::make_shared<internal::WindowsAsyncSocket>(logger);

        if (auto res = s->Initialize(); res.HasErr()) {
            return nullptr;
        }

        if (auto res = s->CreateSocket(netTransport); res.HasErr()) {
            return nullptr;
        }

        // Before bind() - 소켓 옵션 설정
        const SocketHandle sh = s->GetSocketHandle();
        socketOptionBuilder->SetReuseAddr(sh, true);
        socketOptionBuilder->SetSendBufferSize(sh, Const::Buffer::sendBufferSize);

        if (auto res = s->Bind(static_cast<unsigned short>(netCfg.server.port)); res.HasErr()) {
            return nullptr;
        }

        if (auto res = s->Listen(netCfg.server.backlog); res.HasErr()) {
            return nullptr;
        }

        return {s};
    }
}
