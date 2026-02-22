#include "pch.h"

#include "acceptor/IocpAcceptor.h"
#include "socket/SocketOptionBuilder.h"

namespace highp::network {
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

        // 0) 원본 C++17 if with initializer 로 2줄 -> 1줄로 줄이기!
        //if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
        //	return res;
        //}

        // 1) bool 연산자 오버로딩 -> .HasErr() 를 줄일 수 있음.
        //if (auto res = LoadAcceptExFunctions(); !res) {
        //	return res;
        //}
        //
        // 만약 return 으로 err propagation 이 필요없으면
        //if (LoadAcceptExFunctions()) {
        //}

        // 2) macro GUARD(expr)
        GUARD(LoadAcceptExFunctions());

        _logger->Info("IocpAcceptor initialized. listen socket associated with IOCP.");
        return Res::Ok();
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

        _logger->Info("AcceptEx functions loaded successfully.");
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
            return Res::Err(err::ENetworkError::SocketCreateFailed);
        }

        auto* overlapped = mem::HybridObjectPool<AcceptOverlapped>::Get();
        ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
        overlapped->ioType = EIoType::Accept;
        overlapped->clientSocket = acceptSocket;

        {
            std::scoped_lock lock(_ioPendingOverlappedsLock);
            _ioPendingOverlappeds.insert(overlapped);
        }

        DWORD bytesReceived = 0;
        constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

        // overlapped->overlapped이 첫 번째 멤버이므로
        // OverlappedExt* 를 직접 LPOVERLAPPED로 캐스팅 가능
        BOOL result = _fnAcceptEx(
            _listenSocket,
            acceptSocket,
            overlapped->buf,
            0,
            addrLen,
            addrLen,
            &bytesReceived,
            reinterpret_cast<LPOVERLAPPED>(overlapped));
        if (result == FALSE) {
            if (DWORD err = WSAGetLastError(); err != ERROR_IO_PENDING) {
                closesocket(acceptSocket);
                {
                    std::scoped_lock lock(_ioPendingOverlappedsLock);
                    _ioPendingOverlappeds.erase(overlapped);
                }
                mem::HybridObjectPool<AcceptOverlapped>::Return(overlapped);

                _logger->Error("AcceptEx failed. error: {}", err);
                return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
            }
        }

        return Res::Ok();
    }

    IocpAcceptor::Res IocpAcceptor::PostAccepts(int count) {
        for (int i = 0; i < count; ++i) {
            GUARD(PostAccept());
        }
        _logger->Info("Posted {} AcceptEx requests.", count);
        return Res::Ok();
    }

    IocpAcceptor::Res IocpAcceptor::OnAcceptComplete(AcceptOverlapped* overlapped, DWORD bytesTransferred) {
        if (overlapped == nullptr || overlapped->ioType != EIoType::Accept) {
            _logger->Error("Invalid AcceptEx overlapped. actual: {}, expected: Accept.",
                           EIoTypeHelper::ToString(overlapped->ioType));

            {
                std::scoped_lock lock(_ioPendingOverlappedsLock);
                _ioPendingOverlappeds.erase(overlapped);
            }
            mem::HybridObjectPool<AcceptOverlapped>::Return(overlapped);

            return Res::Err(err::ENetworkError::SocketAcceptFailed);
        }

        const auto result = _socketOptionBuilder->SetUpdateAcceptContext(
            overlapped->clientSocket,
            _listenSocket);

        if (result == SOCKET_ERROR) {
            _logger->Error("SO_UPDATE_ACCEPT_CONTEXT failed. error: {}", WSAGetLastError());

            closesocket(overlapped->clientSocket);
            {
                std::scoped_lock lock(_ioPendingOverlappedsLock);
                _ioPendingOverlappeds.erase(overlapped);
            }
            mem::HybridObjectPool<AcceptOverlapped>::Return(overlapped);

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

        {
            std::scoped_lock lock(_ioPendingOverlappedsLock);
            _ioPendingOverlappeds.erase(overlapped);
        }
        mem::HybridObjectPool<AcceptOverlapped>::Return(overlapped);

        // 3) 매크로 처리 + 후속 실행함수 추가
        GUARD_EFFECT(PostAccept(), [this] {
                     _logger->Error("Failed to re-post AcceptEx after completion.");
                     });

        return Res::Ok();
    }

    void IocpAcceptor::Shutdown() {
        _logger->Info("IocpAcceptor shutdown start.");
        {
            std::scoped_lock lock(_ioPendingOverlappedsLock);
            for (auto* o : _ioPendingOverlappeds) {
                CancelIoEx(reinterpret_cast<HANDLE>(_listenSocket),
                           &o->overlapped);
            }
        }
        // _overlappedPool.Clear();
        _fnAcceptEx = nullptr;
        _fnGetAcceptExSockAddrs = nullptr;
        _logger->Info("IocpAcceptor shutdown complete.");
    }
}
