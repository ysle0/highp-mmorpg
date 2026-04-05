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

        if (const ISocket::Res initializeRes = s->Initialize();
            initializeRes.HasErr()) {
            return nullptr;
        }

        if (const ISocket::Res createSocketRes = s->CreateSocket(netTransport);
            createSocketRes.HasErr()) {
            return nullptr;
        }

        // Before bind() - 소켓 옵션 설정
        const SocketHandle sh = s->GetSocketHandle();
        socketOptionBuilder->SetReuseAddr(sh, true);
        socketOptionBuilder->SetSendBufferSize(sh, Const::Buffer::sendBufferSize);

        if (const ISocket::Res bindRes = s->Bind(static_cast<unsigned short>(netCfg.server.port));
            bindRes.HasErr()) {
            return nullptr;
        }

        if (const ISocket::Res listenRes = s->Listen(netCfg.server.backlog);
            listenRes.HasErr()) {
            return nullptr;
        }

        return {s};
    }
}
