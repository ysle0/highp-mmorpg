#pragma once
#include "platform.h"
#include "EIoType.h"
#include "Const.h"

namespace highp::network {
struct OverlappedExt : OVERLAPPED {
	// WSAOVERLAPPED must be first for correct casting to LPWSAOVERLAPPED
	WSAOVERLAPPED overlapped;
	EIoType ioType{ EIoType::Accept };
	SocketHandle clientSocket = InvalidSocket;
	WSABUF wsaBuffer{};
	char buffer[Const::socketBufferSize];

	// Overlapped struct 는 등록되고나서 주소 변경이나 이동이 일어나면
	// mem corruption 이 발생할 수 있으므로 복사 및 이동 금지
	OverlappedExt() = default;
	OverlappedExt(const OverlappedExt&) = delete;
	OverlappedExt& operator=(const OverlappedExt&) = delete;
	OverlappedExt(OverlappedExt&&) = delete;
	OverlappedExt& operator=(OverlappedExt&&) = delete;
};
}
