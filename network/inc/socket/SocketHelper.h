#pragma once
#include "ISocket.h"
#include <logger/Logger.hpp>
#include "config/NetworkCfg.h"
#include "transport/NetworkTransport.hpp"

namespace highp::network {
    class SocketOptionBuilder;

    class SocketHelper {
    public:
        SocketHelper() = delete;

        static std::shared_ptr<ISocket> MakeDefaultListener(
            std::shared_ptr<log::Logger> logger,
            NetworkTransport netTransport,
            NetworkCfg networkCfg,
            std::shared_ptr<SocketOptionBuilder> socketOptionBuilder = nullptr
        );
    };
}
