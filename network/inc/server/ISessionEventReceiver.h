#pragma once
#include <memory>
#include "client/windows/Client.h"
#include <span>

namespace highp::net {
    /// <summary>
    /// 세션 이벤트 수신 인터페이스.
    /// 앱 레이어에서 구현하여 ServerLifeCycle에 주입한다.
    /// </summary>
    struct ISessionEventReceiver {
        virtual ~ISessionEventReceiver() = default;

        /// <summary>새 클라이언트 연결 시 호출</summary>
        virtual void OnAccept(std::shared_ptr<Client> client) = 0;

        /// <summary>데이터 수신 시 호출</summary>
        virtual void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) = 0;

        /// <summary>데이터 송신 완료 시 호출</summary>
        virtual void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) = 0;

        /// <summary>클라이언트 연결 해제 시 호출</summary>
        virtual void OnDisconnect(std::shared_ptr<Client> client) = 0;
    };
}
