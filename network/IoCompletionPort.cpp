#include "pch.h"
#include "IoCompletionPort.h"

namespace highp::network {
IoCompletionPort::IoCompletionPort(RuntimeCfg runtimeCfg) : _runtimeCfg(runtimeCfg) {}

IoCompletionPort::Res IoCompletionPort::Initialize(SocketHandle clientSocket) {
	CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		nullptr,
		_runtimeCfg.workerThreadCount
	);

		return Res::Ok();
}

IoCompletionPort::Res IoCompletionPort::PostAccept() {
	return Res::Ok();
}

IoCompletionPort::Res IoCompletionPort::PostAccepts(int repeat) {
	return Res::Ok();
}

IoCompletionPort::Res IoCompletionPort::OnAcceptComplete(int bytesTransferred, std::vector<std::byte> bytes) {
	return Res::Ok();
}

void IoCompletionPort::SetCallback(std::function<void()> onAfterAcceptComplete) {

}
}
