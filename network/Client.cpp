#include "pch.h"
#include "Client.h"

namespace highp::network {

Client::Client() {
	ZeroMemory(&recvOverlapped, sizeof(RecvOverlapped));
	ZeroMemory(&sendOverlapped, sizeof(SendOverlapped));
}

Client::Res Client::PostRecv() {
	ZeroMemory(&recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	recvOverlapped.bufDesc.buf = recvOverlapped.buf;
	recvOverlapped.bufDesc.len = std::size(recvOverlapped.buf);
	recvOverlapped.ioType = EIoType::Recv;

	DWORD flags = 0;
	DWORD recvNumBytes = 0;

	int result = WSARecv(
		socket,
		&recvOverlapped.bufDesc,
		1,
		&recvNumBytes,
		&flags,
		reinterpret_cast<LPWSAOVERLAPPED>(&recvOverlapped),
		nullptr);

	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
		return Res::Err(err::ENetworkError::WsaRecvFailed);
	}

	return Res::Ok();
}

Client::Res Client::PostSend(std::string_view data) {
	ZeroMemory(&sendOverlapped.overlapped, sizeof(WSAOVERLAPPED));

	auto len = static_cast<ULONG>(data.size());
	CopyMemory(sendOverlapped.buf, data.data(), len);
	sendOverlapped.bufDesc.buf = sendOverlapped.buf;
	sendOverlapped.bufDesc.len = len;
	sendOverlapped.ioType = EIoType::Send;

	DWORD sendNumBytes = 0;

	int result = WSASend(
		socket,
		&sendOverlapped.bufDesc,
		1,
		&sendNumBytes,
		0,
		reinterpret_cast<LPWSAOVERLAPPED>(&sendOverlapped),
		nullptr);

	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
		return Res::Err(err::ENetworkError::WsaSendFailed);
	}

	return Res::Ok();
}

void Client::Close(bool isFireAndForget) {
	//linger linger{ 0, 0 };
	//if (isFireAndForget) {
	//	linger.l_onoff = 1;
	//}


	//setsockopt(socket,
	//	SOL_SOCKET,
	//	SO_LINGER,
	//	(char*)&linger,
	//	sizeof(linger));

	shutdown(socket, SD_BOTH);
	closesocket(socket);

	socket = INVALID_SOCKET;
}

}
