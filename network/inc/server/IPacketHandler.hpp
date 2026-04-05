#pragma once
#include <memory>
#include "client/windows/Client.h"

namespace highp::net {
    /// <summary>
    /// 페이로드 타입별 비즈니스 로직 핸들러 인터페이스.
    /// 앱 레이어에서 구현하여 PacketDispatcher에 등록한다.
    /// </summary>
    template <typename T>
    struct IPacketHandler {
        virtual ~IPacketHandler() = default;

        /// <summary>타입화된 페이로드를 처리한다.</summary>
        virtual void Handle(
            const std::shared_ptr<Client>& client,
            const T* payload
        ) = 0;
    };
}
