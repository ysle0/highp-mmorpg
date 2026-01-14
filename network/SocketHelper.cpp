#include "pch.h"
#include "SocketHelper.h"
#include "WindowsAsyncSocket.h"

namespace highp::network {
std::shared_ptr<ISocket> SocketHelper::MakeDefault(
	std::shared_ptr<log::Logger> logger,
	NetworkTransport netTransport,
	NetworkCfg networkCfg
) {
	auto s{ std::make_shared<WindowsAsyncSocket>(logger) };

	if (auto res = s->Initialize(); res.HasErr()) {
		return nullptr;
	}

	if (auto res = s->CreateSocket(netTransport); res.HasErr()) {
		return nullptr;
	}

	if (auto res = s->Bind(networkCfg.port); res.HasErr()) {
		return nullptr;
	}

	if (auto res = s->Listen(networkCfg.backlog); res.HasErr()) {
		return nullptr;
	}

	return { s };
}
}
