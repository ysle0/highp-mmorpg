#pragma once

#include "platform.h"
#include <functional>
#include <IocpError.h>
#include <memory>
#include <span>
#include "OverlappedExt.h"
#include "RuntimeCfg.h"

namespace highp::network {
class IoCompletionPort final {
	using Res = fn::Result<void, err::EIocpError>;
public:
	explicit IoCompletionPort(RuntimeCfg runtimeCfg);
	Res Initialize(SocketHandle clientSocket);
	Res PostAccept();
	Res PostAccepts(int repeat);
	Res OnAcceptComplete(int bytesTransferred, std::span<char> recvBuffer);
	void SetCallback(std::function<void()> onAfterAcceptComplete);

private:
	RuntimeCfg _runtimeCfg;
	SocketHandle _listenSocket = InvalidSocket;
	std::shared_ptr<IoCompletionPort> _iocp;
	//LPFN_ACCEPTEX _fnAcceptEx;
	//LPFN_GETACCEPTEXSOCKADDRS _fnGetAcceptExSockAddrs;
	std::vector<network::OverlappedExt> _reusedOverlappedExts;
	std::function<void()> _onAccept;
};
}

