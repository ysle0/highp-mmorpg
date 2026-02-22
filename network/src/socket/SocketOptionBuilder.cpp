#include "pch.h"
#include "socket/SocketOptionBuilder.h"
#include <logger/Logger.hpp>

namespace highp::net {
    SocketOptionBuilder::SocketOptionBuilder(
        std::shared_ptr<log::Logger> logger
    ) : _logger(logger) {
    }

    //=============================================================================
    // Before bind() - 소켓 생성 직후
    //=============================================================================

    bool SocketOptionBuilder::SetReuseAddr(SocketHandle sh, bool enable) {
        int value = enable ? 1 : 0;
        if (setsockopt(sh, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&value), sizeof(value)) == SocketError) {
            _logger->Error("SetReuseAddr failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetReuseAddr: {}", enable);
        return true;
    }

    bool SocketOptionBuilder::SetSendBufferSize(SocketHandle sh, int size) {
        if (setsockopt(sh, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<const char*>(&size), sizeof(size)) == SocketError) {
            _logger->Error("SetSendBufferSize failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetSendBufferSize: {}", size);
        return true;
    }

    bool SocketOptionBuilder::SetRecvBufferSize(SocketHandle sh, int size) {
        if (setsockopt(sh, SOL_SOCKET, SO_RCVBUF,
                       reinterpret_cast<const char*>(&size), sizeof(size)) == SocketError) {
            _logger->Error("SetRecvBufferSize failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetRecvBufferSize: {}", size);
        return true;
    }

    bool SocketOptionBuilder::SetBroadcast(SocketHandle sh, bool enable) {
        int value = enable ? 1 : 0;
        if (setsockopt(sh, SOL_SOCKET, SO_BROADCAST,
                       reinterpret_cast<const char*>(&value), sizeof(value)) == SocketError) {
            _logger->Error("SetBroadcast failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetBroadcast: {}", enable);
        return true;
    }

    //=============================================================================
    // After AcceptEx() - 연결 수락 직후
    //=============================================================================

    bool SocketOptionBuilder::SetUpdateAcceptContext(SocketHandle acceptedSock, SocketHandle listenSock) {
        if (setsockopt(acceptedSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                       reinterpret_cast<const char*>(&listenSock), sizeof(listenSock)) == SocketError) {
            _logger->Error("SetUpdateAcceptContext failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetUpdateAcceptContext: applied");
        return true;
    }

    //=============================================================================
    // On connected socket - 연결된 소켓
    //=============================================================================

    bool SocketOptionBuilder::SetTcpNoDelay(SocketHandle sh, bool enable) {
        int value = enable ? 1 : 0;
        if (setsockopt(sh, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&value), sizeof(value)) == SocketError) {
            _logger->Error("SetTcpNoDelay failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetTcpNoDelay: {}", enable);
        return true;
    }

    bool SocketOptionBuilder::SetKeepAlive(SocketHandle sh, const tcp_keepalive& config) {
        int value = config.onoff ? 1 : 0;
        if (setsockopt(sh, SOL_SOCKET, SO_KEEPALIVE,
                       reinterpret_cast<const char*>(&value), sizeof(value)) == SocketError) {
            _logger->Error("SetKeepAlive failed: {}", WSAGetLastError());
            return false;
        }

        if (config.onoff == 0) {
            _logger->Debug("SetKeepAlive: disabled");
            return true;
        }

        DWORD bytesReturned = 0;
        if (WSAIoctl(sh, SIO_KEEPALIVE_VALS,
                     const_cast<tcp_keepalive*>(&config), sizeof(config),
                     nullptr, 0, &bytesReturned, nullptr, nullptr) == SocketError) {
            _logger->Error("SetKeepAlive SIO_KEEPALIVE_VALS failed: {}", WSAGetLastError());
            return false;
        }

        _logger->Debug("SetKeepAlive: time={}ms, interval={}ms",
                       config.keepalivetime, config.keepaliveinterval);
        return true;
    }

    bool SocketOptionBuilder::SetLinger(SocketHandle sh, const linger& l) {
        if (setsockopt(sh, SOL_SOCKET, SO_LINGER,
                       reinterpret_cast<const char*>(&l), sizeof(l)) == SocketError) {
            _logger->Error("SetLinger failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetLinger: onoff={}, timeout={}s", l.l_onoff, l.l_linger);
        return true;
    }

    bool SocketOptionBuilder::SetSendTimeout(SocketHandle sh, DWORD timeoutMs) {
        if (setsockopt(sh, SOL_SOCKET, SO_SNDTIMEO,
                       reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == SocketError) {
            _logger->Error("SetSendTimeout failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetSendTimeout: {}ms", timeoutMs);
        return true;
    }

    bool SocketOptionBuilder::SetRecvTimeout(SocketHandle sh, DWORD timeoutMs) {
        if (setsockopt(sh, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == SocketError) {
            _logger->Error("SetRecvTimeout failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetRecvTimeout: {}ms", timeoutMs);
        return true;
    }

    bool SocketOptionBuilder::SetTtl(SocketHandle sh, int ttl) {
        if (setsockopt(sh, IPPROTO_IP, IP_TTL,
                       reinterpret_cast<const char*>(&ttl), sizeof(ttl)) == SocketError) {
            _logger->Error("SetTtl failed: {}", WSAGetLastError());
            return false;
        }
        _logger->Debug("SetTtl: {}", ttl);
        return true;
    }

    //=============================================================================
    // Utility
    //=============================================================================

    bool SocketOptionBuilder::ApplyGameServerDefaults(SocketHandle sh) {
        _logger->Info("ApplyGameServerDefaults: starting...");

        if (!SetReuseAddr(sh, true)) return false;
        if (!SetTcpNoDelay(sh, true)) return false;

        tcp_keepalive keepAlive = {};
        keepAlive.onoff = 1;
        keepAlive.keepalivetime = 30000;
        keepAlive.keepaliveinterval = 5000;
        if (!SetKeepAlive(sh, keepAlive)) return false;

        linger l = {};
        l.l_onoff = 1;
        l.l_linger = 5;
        if (!SetLinger(sh, l)) return false;

        _logger->Info("ApplyGameServerDefaults: completed");
        return true;
    }
}
