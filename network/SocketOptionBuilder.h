#pragma once
#include "platform.h"

namespace highp::log {
class Logger;
}

namespace highp::network {

/// <summary>소켓 옵션을 설정하는 빌더 클래스</summary>
class SocketOptionBuilder final {
public:
	explicit SocketOptionBuilder(std::shared_ptr<log::Logger> logger);

	// Before bind() - 소켓 생성 직후
	bool SetReuseAddr(SocketHandle sh, bool enable);
	bool SetSendBufferSize(SocketHandle sh, int size);
	bool SetRecvBufferSize(SocketHandle sh, int size);
	bool SetBroadcast(SocketHandle sh, bool enable);  // UDP only

	// After AcceptEx() - 연결 수락 직후
	bool SetUpdateAcceptContext(SocketHandle acceptedSock, SocketHandle listenSock);

	// On connected socket - 연결된 소켓
	bool SetTcpNoDelay(SocketHandle sh, bool enable);
	bool SetKeepAlive(SocketHandle sh, const tcp_keepalive& config);
	bool SetLinger(SocketHandle sh, const linger& l);
	bool SetSendTimeout(SocketHandle sh, DWORD timeoutMs);
	bool SetRecvTimeout(SocketHandle sh, DWORD timeoutMs);
	bool SetTtl(SocketHandle sh, int ttl);

	// Utility
	bool ApplyGameServerDefaults(SocketHandle sh);

private:
	std::shared_ptr<log::Logger> _logger;
};

}
