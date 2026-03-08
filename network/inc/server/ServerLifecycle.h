#pragma once

#include "io/windows/IocpIoMultiplexer.h"
#include "acceptor/IocpAcceptor.h"
#include "client/windows/Client.h"
#include "config/NetworkCfg.h"
#include "ISessionEventReceiver.h"
#include <logger/Logger.hpp>
#include <functional/Result.hpp>
#include <error/NetworkError.h>
#include <memory>
#include <vector>

namespace highp::net
{
    class ISocket;

    /// <summary>
    /// IOCP 기반 서버 공통 로직을 담당하는 클래스.
    /// I/O 멀티플렉서, Acceptor, 클라이언트 관리 등을 캡슐화한다.
    /// </summary>
    /// <remarks>
    /// 앱 서버(Server, ChatServer 등)는 ServerCore를 멤버로 가지고,
    /// ISessionEventReceiver를 구현하여 비즈니스 로직만 처리한다.
    /// </remarks>
    class ServerLifeCycle final
    {
    public:
        using Res = fn::Result<void, err::ENetworkError>;

        /// <summary>
        /// ServerCore 생성자.
        /// </summary>
        /// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
        /// <param name="socketOptionBuilder"></param>
        /// <param name="handler">서버 이벤트 핸들러. 앱 레이어에서 구현.</param>
        explicit ServerLifeCycle(
            std::shared_ptr<log::Logger> logger,
            std::shared_ptr<SocketOptionBuilder> socketOptionBuilder,
            ISessionEventReceiver* handler);

        ~ServerLifeCycle() noexcept;

        ServerLifeCycle(const ServerLifeCycle&) = delete;
        ServerLifeCycle& operator=(const ServerLifeCycle&) = delete;
        ServerLifeCycle(ServerLifeCycle&&) = delete;
        ServerLifeCycle& operator=(ServerLifeCycle&&) = delete;

        /// <summary>
        /// 서버를 시작한다.
        /// </summary>
        /// <param name="listenSocket">Listen 상태의 소켓</param>
        /// <param name="config">서버 네트워크 설정</param>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        Res Start(std::shared_ptr<ISocket> listenSocket, const NetworkCfg& config);

        /// <summary>
        /// 서버를 중지하고 리소스를 정리한다.
        /// </summary>
        void Stop();

        /// <summary>
        /// 클라이언트 연결을 종료한다.
        /// </summary>
        /// <param name="client">종료할 클라이언트</param>
        /// <param name="force">true면 linger 없이 즉시 종료</param>
        void CloseClient(std::shared_ptr<Client> client, bool force = true);

        /// <summary>현재 연결된 클라이언트 수</summary>
        size_t GetConnectedClientCount() const noexcept;

        /// <summary>서버가 실행 중인지 확인</summary>
        bool IsRunning() const noexcept;

    private:
        /// <summary>IOCP 완료 이벤트 핸들러</summary>
        void OnCompletion(internal::CompletionEvent event);

        /// <summary>Accept 후 클라이언트 초기화 (IocpAcceptor 콜백)</summary>
        void SetupClient(internal::AcceptContext& ctx);

        /// <summary>Recv 완료 핸들러</summary>
        void HandleRecv(internal::CompletionEvent& event);

        /// <summary>Send 완료 핸들러</summary>
        void HandleSend(internal::CompletionEvent& event);

        /// <summary>Client 풀에서 사용 가능한 슬롯을 찾는다</summary>
        std::shared_ptr<Client> FindAvailableClient();

        std::shared_ptr<log::Logger> _logger;
        std::shared_ptr<SocketOptionBuilder> _socketOptionBuilder;
        ISessionEventReceiver* _handler = nullptr;

        std::unique_ptr<internal::IocpIoMultiplexer> _iocp;
        std::unique_ptr<internal::IocpAcceptor> _acceptor;

        std::vector<std::shared_ptr<Client>> _clientPool;
        std::atomic<size_t> _connectedClientCount{0};

        NetworkCfg _config;
    };
} // namespace highp::net
