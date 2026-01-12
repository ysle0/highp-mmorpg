#include "pch.h"
#include "Client.h"

namespace highp::network {
Client::Client() {
	ZeroMemory(&recvOverlapped, sizeof(OverlappedExt));
	ZeroMemory(&sendOverlapped, sizeof(OverlappedExt));
}

void Client::Close(bool isFireAndForget) {
	linger linger{ 0, 0 };
	if (isFireAndForget) {
		linger.l_onoff = 1;
	}

	shutdown(socket, SD_BOTH);

	setsockopt(socket,
		SOL_SOCKET,
		SO_LINGER,
		(char*)&linger,
		sizeof(linger));

	closesocket(socket);

	socket = INVALID_SOCKET;
}
}
