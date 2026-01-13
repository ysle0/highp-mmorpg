#pragma once
#include "ISocket.h"
#include <Logger.hpp>

namespace highp::network {

/// <summary>
/// 비동기 소켓 기본 구현 클래스.
/// ISocket 인터페이스의 공통 구현을 제공하며, 플랫폼별 구현체의 기반 클래스로 사용된다.
/// std::enable_shared_from_this를 상속하여 콜백에서 안전하게 shared_ptr 획득 가능.
/// </summary>
class AsyncSocket :
	public ISocket,
	public std::enable_shared_from_this<AsyncSocket> {

public:
	/// <summary>
	/// AsyncSocket 생성자.
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	explicit AsyncSocket(std::shared_ptr<log::Logger> logger);

	/// <summary>가상 소멸자</summary>
	virtual ~AsyncSocket() noexcept = default;

	/// <summary>소켓 라이브러리 초기화</summary>
	ISocket::Res Initialize() override;

	/// <summary>지정된 전송 프로토콜로 소켓 생성</summary>
	ISocket::Res CreateSocket(NetworkTransport) override;

	/// <summary>소켓을 지정된 포트에 바인딩</summary>
	ISocket::Res Bind(unsigned short port) override;

	/// <summary>소켓을 Listen 상태로 전환</summary>
	ISocket::Res Listen(int backlog) override;

	/// <summary>소켓 리소스 정리</summary>
	ISocket::Res Cleanup() override;

	/// <summary>내부 소켓 핸들 반환</summary>
	SocketHandle GetSocketHandle() const {
		return _socketHandle;
	}

protected:
	/// <summary>소켓 핸들</summary>
	SocketHandle _socketHandle;

	/// <summary>로거 인스턴스</summary>
	std::shared_ptr<log::Logger> _logger;
};
}
