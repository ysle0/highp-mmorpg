#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <memory>
#include <string_view>
#include <WinSock2.h>

// Forward declaration
namespace highp::log {
class Logger;
}

namespace highp::echo_cli {
using highp::log::Logger;

/// <summary>
/// Echo 서버에 연결하는 테스트용 클라이언트.
/// 서버에 메시지를 전송하고 응답을 수신한다.
/// </summary>
class EchoClient final {
public:
	/// <summary>
	/// EchoClient 생성자.
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	explicit EchoClient(std::shared_ptr<Logger> logger) : _logger(logger) {}

	/// <summary>
	/// Echo 서버에 연결한다.
	/// </summary>
	/// <param name="ipAddress">서버 IP 주소 (예: "127.0.0.1")</param>
	/// <param name="port">서버 포트 번호</param>
	/// <returns>연결 성공 시 true</returns>
	[[nodiscard]] bool Connect(const char* ipAddress, unsigned short port);

	/// <summary>
	/// 서버 연결을 종료한다.
	/// </summary>
	/// <returns>종료 성공 시 true</returns>
	bool Disconnect();

	/// <summary>
	/// 서버에 메시지를 전송한다.
	/// </summary>
	/// <param name="message">전송할 메시지</param>
	void Send(std::string_view message);

private:
	/// <summary>로거 인스턴스</summary>
	std::shared_ptr<Logger> _logger;

	/// <summary>서버 연결 소켓</summary>
	SOCKET _serverSocket = INVALID_SOCKET;
};
}
