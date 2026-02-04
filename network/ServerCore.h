#pragma once

#include "platform.h"
#include "IocpIoMultiplexer.h"
#include "IocpAcceptor.h"
#include "Client.h"
#include "NetworkCfg.h"
#include <Logger.hpp>
#include <Result.hpp>
#include <NetworkError.h>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace highp::network {

/// <summary>
/// 서버 이벤트 핸들러 인터페이스.
/// 앱 레이어에서 구현하여 ServerCore에 주입한다.
/// </summary>
struct IServerHandler {
	virtual ~IServerHandler() = default;

	/// <summary>새 클라이언트 연결 시 호출</summary>
	virtual void OnAccept(std::shared_ptr<Client> client) = 0;

	/// <summary>데이터 수신 시 호출</summary>
	virtual void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) = 0;

	/// <summary>데이터 송신 완료 시 호출</summary>
	virtual void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) = 0;

	/// <summary>클라이언트 연결 해제 시 호출</summary>
	virtual void OnDisconnect(std::shared_ptr<Client> client) = 0;
};

/// <summary>
/// IOCP 기반 서버 공통 로직을 담당하는 클래스.
/// I/O 멀티플렉서, Acceptor, 클라이언트 관리 등을 캡슐화한다.
/// </summary>
/// <remarks>
/// 앱 서버(EchoServer, ChatServer 등)는 ServerCore를 멤버로 가지고,
/// IServerHandler를 구현하여 비즈니스 로직만 처리한다.
/// </remarks>
class ServerCore final {
public:
	using Res = fn::Result<void, err::ENetworkError>;

	/// <summary>
	/// ServerCore 생성자.
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	/// <param name="handler">서버 이벤트 핸들러. 앱 레이어에서 구현.</param>
	explicit ServerCore(
		std::shared_ptr<log::Logger> logger,
		IServerHandler* handler);

	~ServerCore() noexcept;

	ServerCore(const ServerCore&) = delete;
	ServerCore& operator=(const ServerCore&) = delete;
	ServerCore(ServerCore&&) = delete;
	ServerCore& operator=(ServerCore&&) = delete;

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
	size_t GetConnectedClientCount() const noexcept { return _connectedClientCount.load(); }

	/// <summary>서버가 실행 중인지 확인</summary>
	bool IsRunning() const noexcept { return _iocp && _iocp->IsRunning(); }

private:
	/// <summary>IOCP 완료 이벤트 핸들러</summary>
	void OnCompletion(CompletionEvent event);

	/// <summary>Accept 완료 핸들러</summary>
	void OnAcceptInternal(AcceptContext& ctx);

	/// <summary>Recv 완료 핸들러</summary>
	void HandleRecv(CompletionEvent& event);

	/// <summary>Send 완료 핸들러</summary>
	void HandleSend(CompletionEvent& event);

	/// <summary>Client 풀에서 사용 가능한 슬롯을 찾는다</summary>
	std::shared_ptr<Client> FindAvailableClient();

private:
	std::shared_ptr<log::Logger> _logger;
	IServerHandler* _handler = nullptr;

	std::unique_ptr<IocpIoMultiplexer> _iocp;
	std::unique_ptr<IocpAcceptor> _acceptor;

	std::vector<std::shared_ptr<Client>> _clientPool;
	std::atomic<size_t> _connectedClientCount{0};

	NetworkCfg _config;
};

} // namespace highp::network
