#pragma once

#include "platform.h"
#include <memory>
#include "OverlappedExt.h"

namespace highp::network {

/// <summary>
/// 클라이언트 연결 상태 및 I/O 버퍼를 관리하는 구조체.
/// std::enable_shared_from_this를 상속하여 콜백에서 안전하게 shared_ptr 획득 가능.
/// EchoServer::OnAccept()에서 할당되어 사용된다.
/// </summary>
/// <remarks>
/// 각 클라이언트마다 Recv/Send용 OverlappedExt를 보유하여 동시 I/O 작업을 지원한다.
/// </remarks>
struct Client : public std::enable_shared_from_this<Client> {
	/// <summary>기본 생성자. 소켓을 INVALID_SOCKET으로 초기화.</summary>
	Client();

	/// <summary>
	/// 클라이언트 연결을 종료하고 소켓을 닫는다.
	/// </summary>
	/// <param name="isFireAndForget">true면 linger 없이 즉시 종료</param>
	void Close(bool isFireAndForget);

	/// <summary>클라이언트 소켓 핸들</summary>
	SocketHandle socket = INVALID_SOCKET;

	/// <summary>수신 작업용 OverlappedExt 구조체</summary>
	OverlappedExt recvOverlapped;

	/// <summary>송신 작업용 OverlappedExt 구조체</summary>
	OverlappedExt sendOverlapped;
};

}
