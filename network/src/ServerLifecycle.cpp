#include "pch.h"

#include "server/ServerLifecycle.h"
#include "config/Const.h"
#include "socket/ISocket.h"
#include "client/windows/OverlappedExt.h"
#include <utility>

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

    Client::EventCallbacks ServerLifeCycle::BuildClientCallbacks() {
        return Client::EventCallbacks{
            .onConnected = [this] {
                _connectedClientCount.fetch_add(1, std::memory_order_relaxed);
                if (_callbacks.client.onConnected) {
                    _callbacks.client.onConnected();
                }
            },
            .onDisconnected = [this] {
                if (auto count = _connectedClientCount.load(); count > 0) {
                    _connectedClientCount.fetch_sub(1, std::memory_order_relaxed);
                }
                if (_callbacks.client.onDisconnected) {
                    _callbacks.client.onDisconnected();
                }
            },
            .onRecvPosted = [this] {
                if (_callbacks.client.onRecvPosted) {
                    _callbacks.client.onRecvPosted();
                }
            },
            .onRecvPostFailed = [this] {
                if (_callbacks.client.onRecvPostFailed) {
                    _callbacks.client.onRecvPostFailed();
                }
            },
            .onSendPosted = [this] {
                if (_callbacks.client.onSendPosted) {
                    _callbacks.client.onSendPosted();
                }
            },
            .onSendPostFailed = [this] {
                if (_callbacks.client.onSendPostFailed) {
                    _callbacks.client.onSendPostFailed();
                }
            },
        };
    }

    void ServerLifeCycle::UseCallbacks(EventCallbacks callbacks) noexcept {
        _callbacks = std::move(callbacks);
        if (_iocp) {
            _iocp->UseCallbacks(_callbacks.iocp);
        }
        if (_acceptor) {
            _acceptor->UseCallbacks(_callbacks.acceptor);
        }
        const auto effectiveCallbacks = BuildClientCallbacks();
        for (auto& client : _clientPool) {
            if (client) {
                client->UseCallbacks(effectiveCallbacks);
            }
        }
    }

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
        _iocp->UseCallbacks(_callbacks.iocp);

        const auto workerCount = std::thread::hardware_concurrency() *
            _config.thread.maxWorkerThreadMultiplier;

        if (const internal::IocpIoMultiplexer::Res iocpInitRes = _iocp->Initialize(workerCount);
            iocpInitRes.HasErr()) {
            return iocpInitRes;
        }

        // Acceptor 초기화
        _acceptor = std::make_unique<internal::IocpAcceptor>(
            _logger,
            _socketOptionBuilder,
            std::bind_front(&ServerLifeCycle::SetupClient, this)
        );
        _acceptor->UseCallbacks(_callbacks.acceptor);

        if (const internal::IocpAcceptor::Res acceptorInitRes =
            _acceptor->Initialize(listenSocket->GetSocketHandle(), _iocp->GetHandle());
            acceptorInitRes.HasErr()) {
            return Res::Err(err::ENetworkError::IocpCreateFailed);
        }

        // Client 풀 사전 할당
        _clientPool.reserve(_config.server.maxClients);
        for (int i = 0; i < _config.server.maxClients; ++i) {
            _clientPool.emplace_back(std::make_shared<Client>());
            _clientPool.back()->UseCallbacks(BuildClientCallbacks());
        }

        // Accept 요청 등록
        if (const internal::IocpAcceptor::Res postAcceptsRes = _acceptor->PostAccepts(_config.server.backlog);
            postAcceptsRes.HasErr()) {
            return Res::Err(err::ENetworkError::ThreadAcceptFailed);
        }

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

        client->MarkConnectionEstablished();

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
        if (_callbacks.onRecvCompleted) {
            _callbacks.onRecvCompleted(event.bytesTransferred, event.success && event.bytesTransferred > 0);
        }

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
        if (_callbacks.onSendCompleted) {
            _callbacks.onSendCompleted(event.bytesTransferred, event.success && event.bytesTransferred > 0);
        }

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
