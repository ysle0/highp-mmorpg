#pragma once
#include "AsyncSocket.h"

namespace highp::network {

/// <summary>
/// Windows 플랫폼용 비동기 소켓 구현체.
/// AsyncSocket을 상속하여 Windows Winsock2 API를 사용한 소켓 기능을 제공한다.
/// </summary>
class WindowsAsyncSocket : public AsyncSocket {
	/// <summary>소켓 작업 결과 타입</summary>
	using Res = highp::fn::Result<void, highp::err::ENetworkError>;

	/// <summary>소켓 핸들을 포함한 결과 타입</summary>
	using ResWithData = highp::fn::Result<SOCKET, highp::err::ENetworkError>;

public:
	/// <summary>
	/// WindowsAsyncSocket 생성자.
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	explicit WindowsAsyncSocket(std::shared_ptr<log::Logger> logger);

	virtual ~WindowsAsyncSocket() noexcept override;

public:
	/// <summary>Winsock 초기화. WSAStartup() 호출.</summary>
	virtual Res Initialize() override;

	/// <summary>지정된 전송 프로토콜로 소켓 생성.</summary>
	virtual Res CreateSocket(NetworkTransport transport) override;

	/// <summary>소켓을 지정된 포트에 바인딩.</summary>
	virtual Res Bind(unsigned short port) override;

	/// <summary>소켓을 Listen 상태로 전환.</summary>
	virtual Res Listen(int backlog) override;

	/// <summary>소켓 리소스 정리. closesocket() 및 WSACleanup() 호출.</summary>
	virtual Res Cleanup() override;

private:
	/// <summary>바인딩된 소켓 주소 정보</summary>
	SOCKADDR_IN _sockaddr;
};
}

