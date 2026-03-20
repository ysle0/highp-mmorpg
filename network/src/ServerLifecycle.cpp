#include "pch.h"
#include "server/ServerLifecycle.h"
#include "config/Const.h"
#include "socket/ISocket.h"
#include "client/windows/OverlappedExt.h"

namespace highp::net {
    ServerLifeCycle::ServerLifeCycle(
        std::shared_ptr<log::Logger> logger,
        std::shared_ptr<SocketOptionBuilder> socketOptionBuilder,
        ISessionEventReceiver* handler
    ) : _logger(logger),
        _socketOptionBuilder(socketOptionBuilder),
        _handler(handler) {
    }

    ServerLifeCycle::~ServerLifeCycle() noexcept { Stop(); }

    ServerLifeCycle::Res ServerLifeCycle::Start(
        std::shared_ptr<ISocket> listenSocket,
        const NetworkCfg& config
    ) {
        _config = config;

        // IOCP 초기화
        _iocp = std::make_unique<internal::IocpIoMultiplexer>(
            _logger,
            std::bind_front(&ServerLifeCycle::OnCompletion, this)
        );

        const auto workerCount = std::thread::hardware_concurrency() *
            _config.thread.maxWorkerThreadMultiplier;

        GUARD(_iocp->Initialize(workerCount));

        // Acceptor 초기화
        _acceptor = std::make_unique<internal::IocpAcceptor>(
            _logger,
            _socketOptionBuilder,
            std::bind_front(&ServerLifeCycle::SetupClient, this)
        );

        GUARD_EFFECT(
            _acceptor->Initialize(listenSocket->GetSocketHandle(), _iocp->GetHandle()),
            [this]{ return Res::Err(err::ENetworkError::IocpCreateFailed); }
        );

        // Client 풀 사전 할당
        _clientPool.reserve(_config.server.maxClients);
        for (int i = 0; i < _config.server.maxClients; ++i) {
            _clientPool.emplace_back(std::make_shared<Client>());
        }

        // Accept 요청 등록
        GUARD_EFFECT(_acceptor->PostAccepts(_config.server.backlog), [this]{
                     return Res::Err(err::ENetworkError::ThreadAcceptFailed);
                     });

        _logger->Debug("Server Lifecycle started.");
        return Res::Ok();
    }

    void ServerLifeCycle::Stop() {
        if (_acceptor) {
            _acceptor->Shutdown();
            _acceptor.reset();
        }

        if (_iocp) {
            _iocp->Shutdown();
            _iocp.reset();
        }

        _clientPool.clear();
        _connectedClientCount = 0;
    }

    void ServerLifeCycle::CloseClient(std::shared_ptr<Client> client, bool force) {
        if (client && client->socket != INVALID_SOCKET) {
            client->Close(force);
            --_connectedClientCount;
        }
    }

    size_t ServerLifeCycle::GetConnectedClientCount() const noexcept {
        return _connectedClientCount.load();
    }

    bool ServerLifeCycle::IsRunning() const noexcept {
        return _iocp && _iocp->IsRunning();
    }

    void ServerLifeCycle::OnCompletion(internal::CompletionEvent event) {
        switch (event.ioType) {
        case internal::EIoType::Accept:
            if (_acceptor) {
                auto* overlapped = reinterpret_cast<internal::AcceptOverlapped*>(event.overlapped);
                if (!_acceptor->OnAcceptComplete(overlapped, event.bytesTransferred)) {
                    _logger->Error("Failed to complete AcceptEx.");
                }
            }
            break;

        case internal::EIoType::Recv:
            HandleRecv(event);
            break;

        case internal::EIoType::Send:
            HandleSend(event);
            break;

        case internal::EIoType::Disconnect:
        default:
            _logger->Error("Unknown IO type received.");
            break;
        }
    }

    void ServerLifeCycle::SetupClient(internal::AcceptContext& ctx) {
        const std::shared_ptr<Client> client = FindAvailableClient();
        if (!client) {
            _logger->Error("Client pool full!");
            closesocket(ctx.acceptSocket);
            return;
        }

        client->socket = ctx.acceptSocket;

        if (!_iocp->AssociateSocket<Client>(client->socket, client.get())) {
            _logger->Error("Failed to associate socket with IOCP.");
            client->Close(true);
            return;
        }

        if (!client->PostRecv()) {
            _logger->Error("Failed to post initial Recv.");
            client->Close(true);
            return;
        }

        ++_connectedClientCount;

        char clientIp[Const::Buffer::clientIpBufferSize]{};
        inet_ntop(AF_INET, &ctx.remoteAddr.sin_addr, clientIp, sizeof(clientIp));
        _logger->Info("Client connected. socket: {}, ip: {}", client->socket,
                      clientIp);

        // 앱 레이어에 통지
        if (_handler) {
            _handler->OnAccept(client);
        }
    }

    void ServerLifeCycle::HandleRecv(internal::CompletionEvent& event) {
        auto* client = event.target->CopyTo<Client>();
        if (!client)
            return;

        auto clientPtr = client->shared_from_this();

        // 연결 종료 감지
        if (!event.success || event.bytesTransferred == 0) {
            _logger->Info("[Graceful Disconnect] Socket #{} disconnected.",
                          client->socket);
            if (_handler) {
                _handler->OnDisconnect(clientPtr);
            }
            CloseClient(clientPtr, true);
            return;
        }

        auto* overlapped = reinterpret_cast<internal::RecvOverlapped*>(event.overlapped);
        std::span<const char> data{overlapped->buf, event.bytesTransferred};

        // 앱 레이어에 통지
        if (_handler) {
            _handler->OnRecv(clientPtr, data);
        }

        // 다음 수신을 위해 PostRecv 재발행
        if (!client->PostRecv()) {
            _logger->Error("[HandleRecv] PostRecv failed for socket #{}", client->socket);
            CloseClient(clientPtr, true);
        }
    }

    void ServerLifeCycle::HandleSend(internal::CompletionEvent& event) {
        auto* client = static_cast<Client*>(event.target);
        if (!client)
            return;

        auto clientPtr = client->shared_from_this();

        // 앱 레이어에 통지
        if (_handler) {
            _handler->OnSend(clientPtr, event.bytesTransferred);
        }
    }

    std::shared_ptr<Client> ServerLifeCycle::FindAvailableClient() {
        auto found = std::find_if(_clientPool.begin(), _clientPool.end(),
                                  [](const std::shared_ptr<Client>& c) {
                                      return c->socket == INVALID_SOCKET;
                                  });
        if (found != _clientPool.end()) {
            return *found;
        }
        return nullptr;
    }
} // namespace highp::net
