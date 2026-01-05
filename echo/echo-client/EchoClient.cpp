#include <Logger.hpp>
#include "EchoClient.h"

namespace highp::echo::client {
bool EchoClient::Connect(const char* ipAddress, unsigned short port) {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		_logger->Error("WSAStartup failed. error: {}", WSAGetLastError());
		return false;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET) {
		_logger->Error("Create socket failed. error: {}", WSAGetLastError());
		WSACleanup();
		return false;
	}

	_logger->Info("Connecting to {}:{}", ipAddress, port);

	SOCKADDR_IN addr{
		.sin_family = AF_INET,
		.sin_port = htons(port),
	};
	addr.sin_addr.s_addr = inet_addr(ipAddress);

	if (connect(serverSocket, (SOCKADDR*)&addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		_logger->Error("Connect failed. error: {}", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return false;
	}

	_logger->Info("Connected to {}:{}", ipAddress, port);
	_serverSocket = serverSocket;
	return true;
}

bool EchoClient::Disconnect() {
	if (_serverSocket == INVALID_SOCKET) {
		return false;
	}

	closesocket(_serverSocket);
	_serverSocket = INVALID_SOCKET;
	WSACleanup();
	_logger->Info("Disconnected from server.");
	return true;
}

void EchoClient::Send(std::string_view message) {
	if (_serverSocket == INVALID_SOCKET) {
		_logger->Error("Not connected to server.");
		return;
	}

	const int sendBytes = send(_serverSocket, message.data(), static_cast<int>(message.size()), 0);
	if (sendBytes == SOCKET_ERROR) {
		_logger->Error("Send failed. error: {}", WSAGetLastError());
		return;
	}
	_logger->Info("Sent: {}", message);

	char recvBuf[4096]{ 0, };
	const int recvBytes = recv(_serverSocket, recvBuf, sizeof(recvBuf) - 1, 0);
	if (recvBytes > 0) {
		recvBuf[recvBytes] = '\0';
		_logger->Info("Received: {}", std::string_view(recvBuf, recvBytes));
	} else if (recvBytes == 0) {
		_logger->Info("Connection closed by server.");
	} else {
		_logger->Error("Recv failed. error: {}", WSAGetLastError());
	}
}
};
