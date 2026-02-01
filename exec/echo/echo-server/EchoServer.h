#pragma once
#include <AcceptContext.h>
#include <Acceptor.h>
#include <IoCompletionPort.h>
#include <NetworkError.h>
#include <ISocket.h>
#include <NetworkCfg.h>

namespace highp::network {
struct Client;
class SocketOptionBuilder;
}

namespace highp::log {
class Logger;
}

namespace highp::echo_srv {

/// <summary>
/// IOCP 기반 비동기 Echo 서버.
/// 클라이언트로부터 수신한 메시지를 그대로 반환한다.
/// </summary>
/// <remarks>
/// network::IoCompletionPort와 network::Acceptor를 의존성 주입 방식으로 사용하여 관심사를 분리한다.
/// Send/Recv 로직은 network::Client에 위임한다.
/// </remarks>
class EchoServer final {
	/// <summary>EchoServer 작업 결과 타입</summary>
	using Res = highp::fn::Result<void, highp::err::ENetworkError>;

public:
	~EchoServer() noexcept;

	/// <summary>
	/// 기본 설정으로 EchoServer를 생성한다. network config 은 WithDefaults().
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	/// <param name="socketOptionBuilder">소켓 옵션 빌더 (선택적)</param>
	explicit EchoServer(
		std::shared_ptr<log::Logger> logger,
		std::shared_ptr<network::SocketOptionBuilder> socketOptionBuilder = nullptr);

	/// <summary>
	/// 지정된 설정으로 EchoServer를 생성한다.
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	/// <param name="config">서버 네트워크 설정. network::NetworkCfg 참조.</param>
	/// <param name="socketOptionBuilder">소켓 옵션 빌더 (선택적)</param>
	EchoServer(
		std::shared_ptr<log::Logger> logger,
		network::NetworkCfg config,
		std::shared_ptr<network::SocketOptionBuilder> socketOptionBuilder = nullptr);

	/// <summary>
	/// Echo 서버를 시작한다.
	/// </summary>
	/// <param name="listenSocket">Listen 상태의 소켓. network::ISocket 구현체.</param>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// 1. network::IoCompletionPort 초기화
	/// 2. network::Acceptor 초기화 (IOCP 핸들 주입)
	/// 3. CompletionHandler, AcceptCallback 콜백 등록
	/// 4. Client 풀 사전 할당
	/// 5. PostAccepts() 호출
	/// </remarks>
	Res Start(std::shared_ptr<network::ISocket> listenSocket);

	/// <summary>
	/// Echo 서버를 중지하고 리소스를 정리한다.
	/// </summary>
	void Stop();

private:
	/// <summary>
	/// IOCP 완료 이벤트 핸들러. Worker 스레드에서 호출된다.
	/// </summary>
	/// <param name="event">완료 이벤트 정보. network::CompletionEvent 참조.</param>
	/// <remarks>
	/// ioType에 따라 분기:
	/// - Accept: network::Acceptor::OnAcceptComplete() 호출
	/// - Recv: Echo 로직 (Client::PostSend 호출)
	/// - Send: 로깅
	/// </remarks>
	void OnCompletion(network::CompletionEvent event);

	/// <summary>
	/// 새 클라이언트 연결 처리 핸들러.
	/// </summary>
	/// <param name="ctx">Accept 완료 정보. network::AcceptContext 참조.</param>
	/// <remarks>
	/// 1. Client 풀에서 빈 슬롯 할당
	/// 2. 클라이언트 소켓을 IOCP에 연결
	/// 3. Client::PostRecv() 호출
	/// </remarks>
	void OnAccept(network::AcceptContext& ctx);

	/// <summary>
	/// 클라이언트 연결을 종료한다.
	/// </summary>
	/// <param name="client">종료할 클라이언트</param>
	/// <param name="forceClose">true면 linger 없이 즉시 종료</param>
	void CloseClient(std::shared_ptr<network::Client> client, bool forceClose);

	/// <summary>
	/// Client 풀에서 사용 가능한 슬롯을 찾는다.
	/// </summary>
	/// <returns>사용 가능한 Client. 풀이 가득 차면 nullptr.</returns>
	std::shared_ptr<network::Client> FindAvailableClient();

private:
	/// <summary>로거 인스턴스</summary>
	std::shared_ptr<log::Logger> _logger;

	std::shared_ptr<highp::network::ISocket> _listenSocket;

	/// <summary>소켓 옵션 빌더</summary>
	std::shared_ptr<network::SocketOptionBuilder> _socketOptionBuilder;

	/// <summary>서버 네트워크 설정</summary>
	network::NetworkCfg _config = network::NetworkCfg::WithDefaults();

	/// <summary>IOCP 관리자. network::IoCompletionPort 인스턴스.</summary>
	std::unique_ptr<network::IoCompletionPort> _iocp;

	/// <summary>비동기 Accept 관리자. network::Acceptor 인스턴스.</summary>
	std::unique_ptr<network::Acceptor> _acceptor;

	/// <summary>클라이언트 연결 풀. 사전 할당하여 메모리 재사용.</summary>
	std::vector<std::shared_ptr<network::Client>> _clientPool;

	/// <summary>현재 연결된 클라이언트 수</summary>
	std::atomic_uint _connectedClientCount = 0;
};

}
