#pragma once
#include "platform.h"

namespace highp::network {

/// <summary>
/// AcceptEx 완료 후 전달되는 연결 정보 구조체.
/// Acceptor::OnAcceptComplete()에서 생성되어 AcceptCallback으로 전달된다.
/// </summary>
struct AcceptContext {
	/// <summary>AcceptEx로 수락된 클라이언트 소켓 핸들</summary>
	SocketHandle acceptSocket = InvalidSocket;

	/// <summary>Listen 소켓 핸들. SO_UPDATE_ACCEPT_CONTEXT 설정에 사용됨.</summary>
	SocketHandle listenSocket = InvalidSocket;

	/// <summary>서버 측 로컬 주소 정보</summary>
	SOCKADDR_IN localAddr{};

	/// <summary>클라이언트 측 원격 주소 정보</summary>
	SOCKADDR_IN remoteAddr{};
};

}
