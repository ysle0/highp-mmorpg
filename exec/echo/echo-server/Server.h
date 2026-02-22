#pragma once
#include <ISocket.h>
#include <NetworkCfg.h>
#include <NetworkError.h>
#include <ServerLifecycle.h>

using namespace highp;

namespace highp::log {
    class Logger;
}

/// <summary>
/// IOCP 기반 비동기 Echo 서버.
/// 클라이언트로부터 수신한 메시지를 그대로 반환한다.
/// </summary>
/// <remarks>
/// network::ServerCore를 멤버로 가지며, network::IServerHandler를 구현하여
/// Echo 비즈니스 로직만 처리한다.
/// </remarks>
class Server final : public network::IServerHandler {
    /// <summary>Server 작업 결과 타입</summary>
    using Res = fn::Result<void, err::ENetworkError>;

public:
    ~Server() noexcept override;

    /// <summary>
    /// 지정된 설정으로 EchoServer를 생성한다.
    /// </summary>
    /// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
    /// <param name="config">서버 네트워크 설정. network::NetworkCfg 참조.</param>
    /// <param name="socketOptionBuilder">소켓 옵션 빌더 (선택적)</param>
    explicit Server(std::shared_ptr<log::Logger> logger,
                    network::NetworkCfg config,
                    std::shared_ptr<network::SocketOptionBuilder> socketOptionBuilder = nullptr);

    /// <summary>
    /// Echo 서버를 시작한다.
    /// </summary>
    /// <param name="listenSocket">Listen 상태의 소켓. network::ISocket
    /// 구현체.</param> <returns>성공 시 Ok, 실패 시 에러 코드</returns>
    Res Start(std::shared_ptr<network::ISocket> listenSocket);

    /// <summary>
    /// Echo 서버를 중지하고 리소스를 정리한다.
    /// </summary>
    void Stop();

private:
    // IServerHandler 구현
    void OnAccept(std::shared_ptr<network::Client> client) override;

    void OnRecv(std::shared_ptr<network::Client> client,
                std::span<const char> data) override;

    void OnSend(std::shared_ptr<network::Client> client,
                size_t bytesTransferred) override;

    void OnDisconnect(std::shared_ptr<network::Client> client) override;

    /// <summary>로거 인스턴스</summary>
    std::shared_ptr<log::Logger> _logger;

    /// <summary>
    /// 현재 서버를 호스팅하는 소켓.
    /// </summary>
    std::shared_ptr<network::ISocket> _listenSocket;

    /// <summary>소켓 옵션 빌더</summary>
    std::shared_ptr<network::SocketOptionBuilder> _socketOptionBuilder;

    /// <summary>서버 네트워크 설정</summary>
    network::NetworkCfg _config;

    /// <summary>서버 코어. 공통 네트워크 로직 담당.</summary>
    std::unique_ptr<network::ServerLifeCycle> _lifecycle;
};
