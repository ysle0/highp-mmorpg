#include "pch.h"
#include "client/WsaSession.h"

#include <logger/Logger.hpp>

namespace highp::net {
    WsaSession::WsaSession(std::shared_ptr<log::Logger> logger)
        : _logger(logger->WithPrefix("[WsaSession] ")) {
        //
    }

    WsaSession::ResWithSession WsaSession::Create(std::shared_ptr<log::Logger> logger) {
        auto session = std::make_shared<WsaSession>(logger);
        if (auto res = session->Initialize(); res.HasErr()) {
            return ResWithSession::Err(res.Err());
        }

        return ResWithSession::Ok(std::move(session));
    }

    WsaSession::~WsaSession() noexcept {
        Cleanup();
    }

    WsaSession::Res WsaSession::Initialize() {
        if (_isInitialized) {
            return Res::Ok();
        }

        WSADATA wsaData{0};
        const int startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (startupResult != 0) {
            _logger->Error("{}: {}", err::toString(err::ENetworkError::WsaStartupFailed), startupResult);
            return Res::Err(err::ENetworkError::WsaStartupFailed);
        }

        _isInitialized = true;
        return Res::Ok();
    }

    void WsaSession::Cleanup() noexcept {
        if (!_isInitialized) {
            return;
        }

        WSACleanup();
        _isInitialized = false;
    }
}
