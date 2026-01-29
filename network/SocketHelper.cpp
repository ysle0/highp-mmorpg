#include "pch.h"
#include "SocketHelper.h"
#include "WindowsAsyncSocket.h"
#include "SocketOptionBuilder.h"

namespace highp::network {
std::shared_ptr<ISocket> SocketHelper::MakeDefaultListener(
	std::shared_ptr<log::Logger> logger,
	NetworkTransport netTransport,
	NetworkCfg networkCfg,
	std::shared_ptr<SocketOptionBuilder> socketOptionBuilder
) {
	auto s{ std::make_shared<WindowsAsyncSocket>(logger) };

	if (auto res = s->Initialize(); res.HasErr()) {
		return nullptr;
	}

	if (auto res = s->CreateSocket(netTransport); res.HasErr()) {
		return nullptr;
	}

	// Before bind() - 소켓 옵션 설정
	const SocketHandle sh = s->GetSocketHandle();
	socketOptionBuilder->SetReuseAddr(sh, true);
	//socketOptionBuilder->SetSendBufferSize(sh, network::Const::Buffer::sendBufferSize);

	if (auto res = s->Bind(networkCfg.server.port); res.HasErr()) {
		return nullptr;
	}

	if (auto res = s->Listen(networkCfg.server.backlog); res.HasErr()) {
		return nullptr;
	}

	return { s };
}
}
