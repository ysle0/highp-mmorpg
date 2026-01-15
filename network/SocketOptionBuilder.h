#pragma once
#include "platform.h"
#include "mswsockdef.h"

namespace highp::log {
class Logger;
}

namespace highp::network {

/// <summary>소켓 옵션을 설정하는 빌더 클래스</summary>
class SocketOptionBuilder final {
public:
	explicit SocketOptionBuilder(std::shared_ptr<log::Logger> logger);

	// SOL_SOCKET 레벨 옵션
	bool SetReuseAddr(SocketHandle sh, bool enable);
	bool SetKeepAlive(SocketHandle sh, const tcp_keepalive& config);
	bool SetLinger(SocketHandle sh, const linger& l);
	bool SetSendBufferSize(SocketHandle sh, int size);
	bool SetRecvBufferSize(SocketHandle sh, int size);
	bool SetSendTimeout(SocketHandle sh, DWORD timeoutMs);
	bool SetRecvTimeout(SocketHandle sh, DWORD timeoutMs);
	bool SetBroadcast(SocketHandle sh, bool enable);
	bool SetUpdateAcceptContext(SocketHandle acceptedSock, SocketHandle listenSock);

	// IPPROTO_TCP 레벨 옵션
	bool SetTcpNoDelay(SocketHandle sh, bool enable);

	// IPPROTO_IP 레벨 옵션
	bool SetTtl(SocketHandle sh, int ttl);

	// 유틸리티: 게임 서버용 기본 설정 적용
	bool ApplyGameServerDefaults(SocketHandle sh);

private:
	std::shared_ptr<log::Logger> _logger;
};

}
