#pragma once

#include <memory>
#include <NetworkError.h>
#include <Result.hpp>

namespace highp::log {
    class Logger;
}

namespace highp::network {
    class WsaSession final {
    public:
        using Res = fn::Result<void, err::ENetworkError>;
        using ResWithSession = fn::Result<std::shared_ptr<WsaSession>, err::ENetworkError>;

        static ResWithSession Create(std::shared_ptr<log::Logger> logger);

        explicit WsaSession(std::shared_ptr<log::Logger> logger);

        ~WsaSession() noexcept;

        WsaSession(const WsaSession&) = delete;
        WsaSession& operator=(const WsaSession&) = delete;
        WsaSession(WsaSession&&) = delete;
        WsaSession& operator=(WsaSession&&) = delete;

        Res Initialize();
        void Cleanup() noexcept;

    private:
        std::shared_ptr<log::Logger> _logger;
        bool _isInitialized = false;
    };
}
