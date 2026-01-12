#include "pch.h"
#include "WindowAsyncAcceptor.h"

namespace highp::network {
WindowsAsyncAcceptor::WindowsAsyncAcceptor(
	std::shared_ptr<log::Logger> logger,
	unsigned prewarmAcceptorThreadCount
) :
	_logger(logger),
	_prewarmAcceptorThreadCount(prewarmAcceptorThreadCount)
{
	for (auto i = 0; i < prewarmAcceptorThreadCount; ++i) {
		_acceptorThreadsPool.emplace_back(&WindowsAsyncAcceptor::PostAccept, this);
	}
}

WindowsAsyncAcceptor::~WindowsAsyncAcceptor()
{
	// TODO: cleanup resources
}

WindowsAsyncAcceptor::Res WindowsAsyncAcceptor::Initialize(
	SocketHandle listenrSocket,
	SocketHandle clientSocket,
	SOCKADDR_IN& clientAddr)
{
	constexpr int clientAddrLen = sizeof(clientAddr) + 16;
	LPDWORD recvBytesPtr = nullptr;
	LPOVERLAPPED overlappedPtr = nullptr;

	// Should I reuse
	//if (BOOL res = AcceptEx(
	//	listenrSocket,
	//	clientSocket,
	//	&clientAddr,
	//	0,
	//	clientAddrLen,
	//	clientAddrLen,
	//	recvBytesPtr,
	//	overlappedPtr); res != 0) {
	//	return err::LogErrorWithResult<err::ESocketError::AcceptFailed>(_logger);
	//}
	return Res::Ok();
}

WindowsAsyncAcceptor::Res WindowsAsyncAcceptor::PostAccept()
{
	return Res::Ok();
}

WindowsAsyncAcceptor::Res WindowsAsyncAcceptor::PostAcceptMany(unsigned batchCount)
{
	return Res::Ok();
}
}

