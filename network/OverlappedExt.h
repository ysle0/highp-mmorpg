#pragma once
#include "platform.h"
#include "EIoType.h"
#include "Const.h"

namespace highp::network {
struct OverlappedExt {
	// WSAOVERLAPPED must be FIRST member for correct casting
	// reinterpret_cast<OverlappedExt*>(lpOverlapped) 가 올바르게 동작하려면
	// overlapped 멤버가 구조체 첫 번째에 위치해야 함
	WSAOVERLAPPED overlapped{};
	EIoType ioType{ EIoType::Accept };
	SocketHandle clientSocket = InvalidSocket;
	WSABUF wsaBuffer{};
	char buffer[Const::socketBufferSize]{};

	// Overlapped struct 는 등록되고나서 주소 변경이나 이동이 일어나면
	// mem corruption 이 발생할 수 있으므로 복사 및 이동 금지
	OverlappedExt() = default;
	OverlappedExt(const OverlappedExt&) = delete;
	OverlappedExt& operator=(const OverlappedExt&) = delete;
	OverlappedExt(OverlappedExt&&) = delete;
	OverlappedExt& operator=(OverlappedExt&&) = delete;
};
}
