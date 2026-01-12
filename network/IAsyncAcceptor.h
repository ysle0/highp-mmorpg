#pragma once
#include <macro.h>
#include "framework.h"
#include <Result.hpp>
#include <SocketError.h>

namespace highp::network {
class IAsyncAcceptor {
	using Res = highp::fn::Result<void, highp::err::ESocketError>;
public:
	virtual ~IAsyncAcceptor() noexcept = default;
	virtual Res Initialize(
		SocketHandle listenSocket,
		SocketHandle clientSocket,
		SOCKADDR_IN& clientAddr) PURE;
	virtual Res PostAccept() PURE;
	virtual Res PostAcceptMany(unsigned batchCount) PURE;
};
}

