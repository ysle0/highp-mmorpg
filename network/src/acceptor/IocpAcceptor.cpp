#include "pch.h"

#include "acceptor/IocpAcceptor.h"

#include "memory/HybridObjectPool.hpp"
#include "scope/DeferContext.hpp"
#include "socket/SocketOptionBuilder.h"

namespace highp::net::internal {
    IocpAcceptor::IocpAcceptor(
        std::shared_ptr<log::Logger> logger,
        std::shared_ptr<SocketOptionBuilder> socketOptionsBuilder,
        AcceptCallback onAfterAccept
    ) : _logger(logger)
        , _socketOptionBuilder(socketOptionsBuilder)
        , _acceptCallback(std::move(onAfterAccept)) {
    }

    IocpAcceptor::~IocpAcceptor() {
        Shutdown();
    }

    IocpAcceptor::Res IocpAcceptor::Initialize(SocketHandle listenSocket, HANDLE iocpHandle) {
        _listenSocket = listenSocket;
        _iocpHandle = iocpHandle;

        // Listen 소켓을 IOCP에 연결 (AcceptEx 완료 통지를 받기 위해 필수)
        HANDLE result = CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(_listenSocket),
            _iocpHandle,
            0, // Listen 소켓은 completionKey 불필요
            0);
        if (result == nullptr || result != _iocpHandle) {
            _logger->Error("Failed to associate listen socket with IOCP. error: {}", GetLastError());
            return Res::Err(err::ENetworkError::SocketCreateFailed);
        }

        // 2) macro GUARD(expr)
        GUARD(LoadAcceptExFunctions());

        _logger->Debug("IocpAcceptor initialized. listen socket associated with IOCP.");
        return Res::Ok();
    }

    void IocpAcceptor::UseCallbacks(EventCallbacks callbacks) noexcept {
        _callbacks = std::move(callbacks);
    }

    IocpAcceptor::Res IocpAcceptor::LoadAcceptExFunctions() {
        GUID guidAcceptEx = WSAID_ACCEPTEX;
        GUID guidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
        DWORD bytes = 0;

        int result = WSAIoctl(
            _listenSocket,
            SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidAcceptEx,
            sizeof(guidAcceptEx),
            &_fnAcceptEx,
            sizeof(_fnAcceptEx),
            &bytes,
            nullptr,
            nullptr);

        if (result == SOCKET_ERROR) {
            _logger->Error("Failed to load AcceptEx function. error: {}", WSAGetLastError());
            return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
        }

        result = WSAIoctl(
            _listenSocket,
            SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidGetAcceptExSockAddrs,
            sizeof(guidGetAcceptExSockAddrs),
            &_fnGetAcceptExSockAddrs,
            sizeof(_fnGetAcceptExSockAddrs),
            &bytes,
            nullptr,
            nullptr);

        if (result == SOCKET_ERROR) {
            _logger->Error("Failed to load GetAcceptExSockaddrs function. error: {}", WSAGetLastError());
            return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
        }

        _logger->Debug("AcceptEx functions loaded successfully.");
        return Res::Ok();
    }

    IocpAcceptor::Res IocpAcceptor::PostAccept() {
        SocketHandle acceptSocket = WSASocketW(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP,
            nullptr,
            0,
            WSA_FLAG_OVERLAPPED);
        if (acceptSocket == InvalidSocket) {
            _logger->Error("Failed to create accept socket. error: {}", WSAGetLastError());
            if (_callbacks.onAcceptPostFailed) {
                _callbacks.onAcceptPostFailed();
            }
            return Res::Err(err::ENetworkError::SocketCreateFailed);
        }
        highp::scope::DeferContext<SocketHandle> acceptSocketCleanup(&acceptSocket, [](SocketHandle* socket) {
            if (*socket != InvalidSocket) {
                closesocket(*socket);
            }
        });

        auto overlappedItem = mem::HybridObjectPool<AcceptOverlapped>::Get();
        auto* acceptOverlapped = overlappedItem.Get();
        ZeroMemory(&acceptOverlapped->overlapped, sizeof(WSAOVERLAPPED));
        acceptOverlapped->ioType = EIoType::Accept;
        acceptOverlapped->clientSocket = acceptSocket;

        DWORD bytesReceived = 0;
        constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

        // overlapped->overlapped이 첫 번째 멤버이므로
        // OverlappedExt* 를 직접 LPOVERLAPPED로 캐스팅 가능
        BOOL result = _fnAcceptEx(
            _listenSocket,
            acceptSocket,
            acceptOverlapped->buf,
            0,
            addrLen,
            addrLen,
            &bytesReceived,
            reinterpret_cast<LPOVERLAPPED>(acceptOverlapped));
        if (result == FALSE) {
            if (DWORD err = WSAGetLastError(); err != ERROR_IO_PENDING) {
                _logger->Error("AcceptEx failed. error: {}", err);
                if (_callbacks.onAcceptPostFailed) {
                    _callbacks.onAcceptPostFailed();
                }
                return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
            }
        }

        if (_callbacks.onAcceptPosted) {
            _callbacks.onAcceptPosted();
        }

        // 성공/IO_PENDING: holder로 소유권 이전
        acceptSocket = InvalidSocket;
        _acceptOverlappedHolder.Hold(std::move(overlappedItem));
        // _logger->Debug("Posted AcceptEx request.");
        return Res::Ok();
    }

    IocpAcceptor::Res IocpAcceptor::PostAccepts(int count) {
        for (int i = 0; i < count; ++i) {
            GUARD(PostAccept());
        }
        _logger->Debug("Posted {} AcceptEx requests.", count);
        return Res::Ok();
    }

    IocpAcceptor::Res IocpAcceptor::OnAcceptComplete(AcceptOverlapped* overlapped, DWORD bytesTransferred) {
        if (overlapped == nullptr || overlapped->ioType != EIoType::Accept) {
            _logger->Error("Invalid AcceptEx overlapped. expected: Accept.");
            if (_callbacks.onAcceptCompletionFailed) {
                _callbacks.onAcceptCompletionFailed();
            }
            return Res::Err(err::ENetworkError::SocketAcceptFailed);
        }

        // holder에서 꺼냄 → 스코프 종료 시 자동 풀 반환
        const auto overlappedItem = _acceptOverlappedHolder.Release(overlapped);
        if (!overlappedItem) {
            _logger->Error("AcceptOverlapped not found in holder.");
            if (_callbacks.onAcceptCompletionFailed) {
                _callbacks.onAcceptCompletionFailed();
            }
            return Res::Err(err::ENetworkError::SocketAcceptFailed);
        }

        const bool result = _socketOptionBuilder->SetUpdateAcceptContext(
            overlapped->clientSocket,
            _listenSocket);
        highp::scope::DeferContext<SocketHandle> clientSocketCleanup(&overlapped->clientSocket, [](SocketHandle* socket) {
            if (*socket != InvalidSocket) {
                closesocket(*socket);
            }
        });

        if (!result) {
            _logger->Error("SO_UPDATE_ACCEPT_CONTEXT failed. error: {}", WSAGetLastError());
            if (_callbacks.onAcceptCompletionFailed) {
                _callbacks.onAcceptCompletionFailed();
            }
            return Res::Err(err::ENetworkError::SocketAcceptFailed);
        }

        SOCKADDR_IN* localAddr = nullptr;
        SOCKADDR_IN* remoteAddr = nullptr;
        int localAddrLen = 0;
        int remoteAddrLen = 0;
        constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

        _fnGetAcceptExSockAddrs(
            overlapped->buf,
            0,
            addrLen,
            addrLen,
            reinterpret_cast<SOCKADDR**>(&localAddr),
            &localAddrLen,
            reinterpret_cast<SOCKADDR**>(&remoteAddr),
            &remoteAddrLen);

        if (_acceptCallback) {
            AcceptContext ctx{
                .acceptSocket = overlapped->clientSocket,
                .listenSocket = _listenSocket,
                .localAddr = localAddr ? *localAddr : SOCKADDR_IN{},
                .remoteAddr = remoteAddr ? *remoteAddr : SOCKADDR_IN{}
            };
            _acceptCallback(ctx);
        }
        overlapped->clientSocket = InvalidSocket;

        if (_callbacks.onAcceptCompleted) {
            _callbacks.onAcceptCompleted();
        }

        // 3) 매크로 처리 + 후속 실행함수 추가
        GUARD_EFFECT(PostAccept(), [this] {
                     _logger->Error("Failed to re-post AcceptEx after completion.");
                     });

        return Res::Ok();
    }

    void IocpAcceptor::Shutdown() {
        _logger->Debug("IocpAcceptor shutdown start.");

        {
            _acceptOverlappedHolder.ForEach([this](AcceptOverlapped* x) {
                CancelIoEx(reinterpret_cast<HANDLE>(_listenSocket),
                           &x->overlapped);
            });
        }
        // _overlappedPool.Clear();
        _fnAcceptEx = nullptr;
        _fnGetAcceptExSockAddrs = nullptr;

        _logger->Debug("IocpAcceptor shutdown complete.");
    }
}
