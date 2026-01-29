#pragma once

#include "platform.h"
#include "AcceptContext.h"
#include "OverlappedExt.h"
#include <Logger.hpp>
#include <ObjectPool.hpp>
#include <Result.hpp>
#include <NetworkError.h>
#include <functional>
#include <memory>

namespace highp::network {

class SocketOptionBuilder;

/// <summary>
/// Accept 완료 시 호출되는 콜백 함수 타입.
/// Acceptor::SetAcceptCallback()으로 등록하며, AcceptContext를 인자로 받는다.
/// </summary>
using AcceptCallback = std::function<void(AcceptContext&)>;

/// <summary>
/// AcceptEx 기반 비동기 연결 수락을 담당하는 클래스.
/// WSAIoctl로 AcceptEx 함수 포인터를 획득하여 간접 호출 방식을 사용한다.
/// </summary>
/// <remarks>
/// 사용 순서:
/// 1. Initialize()로 Listen 소켓을 IOCP에 연결하고 AcceptEx 함수 로드
/// 2. SetAcceptCallback()으로 연결 수락 콜백 등록
/// 3. PostAccepts()로 비동기 Accept 요청
/// 4. IOCP Worker에서 Accept 완료 시 OnAcceptComplete() 호출
/// 5. Shutdown()으로 정리
/// </remarks>
class Acceptor final {
public:
	/// <summary>Acceptor 작업 결과 타입</summary>
	using Res = fn::Result<void, err::ENetworkError>;

	/// <summary>
	/// Acceptor 생성자.
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	/// <param name="socketOptionBuilder">소켓 옵션 설정을 위한 빌더 (선택적)</param>
	/// <param name="preAllocCount">사전 할당할 Overlapped 개수. 기본값 10.</param>
	/// <param name="onAfterAccept">Accept 완료 콜백 (선택적). RAII 원칙에 따라 생성 시 설정 권장.</param>
	explicit Acceptor(
		std::shared_ptr<log::Logger> logger,
		std::shared_ptr<SocketOptionBuilder> socketOptionBuilder = nullptr,
		int preAllocCount = 10,
		AcceptCallback onAfterAccept = nullptr);

	/// <summary>
	/// 소멸자. Shutdown()을 호출하여 리소스 정리.
	/// </summary>
	~Acceptor() noexcept;

	Acceptor(const Acceptor&) = delete;
	Acceptor& operator=(const Acceptor&) = delete;
	Acceptor(Acceptor&&) = delete;
	Acceptor& operator=(Acceptor&&) = delete;

	/// <summary>
	/// Acceptor를 초기화한다.
	/// Listen 소켓을 IOCP에 연결하고 AcceptEx 함수 포인터를 로드한다.
	/// </summary>
	/// <param name="listenSocket">Listen 상태의 소켓 핸들</param>
	/// <param name="iocpHandle">IoCompletionPort::GetHandle()에서 얻은 IOCP 핸들</param>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// Listen 소켓을 IOCP에 연결해야 AcceptEx 완료 통지를 받을 수 있다.
	/// </remarks>
	Res Initialize(SocketHandle listenSocket, HANDLE iocpHandle);

	/// <summary>
	/// AcceptEx를 호출하여 비동기 Accept 요청을 등록한다.
	/// </summary>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// Accept용 소켓을 생성하고 AcceptEx를 호출한다.
	/// 완료 시 OnAcceptComplete()에서 처리된다.
	/// </remarks>
	Res PostAccept();

	/// <summary>
	/// 여러 개의 AcceptEx 요청을 등록한다.
	/// </summary>
	/// <param name="count">등록할 Accept 요청 수</param>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// 서버 시작 시 backlog 수만큼 사전 등록하여 동시 연결 요청에 대비한다.
	/// </remarks>
	Res PostAccepts(int count);

	/// <summary>
	/// AcceptEx 완료 시 호출되는 핸들러. IoCompletionPort::WorkerLoop()에서 호출된다.
	/// </summary>
	/// <param name="overlapped">완료된 AcceptOverlapped 포인터</param>
	/// <param name="bytesTransferred">전송된 바이트 수 (AcceptEx에서는 보통 0)</param>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// SO_UPDATE_ACCEPT_CONTEXT 설정 후 GetAcceptExSockAddrs로 주소를 파싱하고
	/// 등록된 콜백을 호출한다. 처리 후 자동으로 PostAccept()를 재호출한다.
	/// </remarks>
	Res OnAcceptComplete(AcceptOverlapped* overlapped, DWORD bytesTransferred);

	/// <summary>
	/// Accept 완료 시 호출될 콜백을 등록한다.
	/// </summary>
	/// <param name="onAfterAccept">연결 수락 시 호출될 콜백 함수</param>
	void SetAcceptCallback(AcceptCallback callback);

	/// <summary>
	/// Acceptor를 종료하고 리소스를 정리한다.
	/// </summary>
	void Shutdown();

private:
	/// <summary>
	/// WSAIoctl로 AcceptEx 및 GetAcceptExSockAddrs 함수 포인터를 획득한다.
	/// </summary>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// AcceptEx를 직접 호출하지 않고 WSAIoctl로 함수 포인터를 획득하여 간접 호출한다.
	/// 이는 Winsock 서비스 프로바이더 독립성을 위한 권장 방식이다.
	/// </remarks>
	Res LoadAcceptExFunctions();

	/// <summary>
	/// Accept용 소켓을 생성한다.
	/// </summary>
	/// <returns>생성된 소켓 핸들. 실패 시 InvalidSocket.</returns>
	/// <remarks>
	/// WSA_FLAG_OVERLAPPED 플래그로 비동기 I/O 가능한 소켓을 생성한다.
	/// </remarks>
	SocketHandle CreateAcceptSocket();

	/// <summary>
	/// 풀에서 사용 가능한 AcceptOverlapped를 획득한다.
	/// </summary>
	/// <returns>사용 가능한 AcceptOverlapped 포인터. 풀이 비어있으면 nullptr.</returns>
	AcceptOverlapped* AcquireOverlapped();

	/// <summary>
	/// 사용 완료된 AcceptOverlapped를 풀에 반환한다.
	/// </summary>
	/// <param name="overlapped">반환할 AcceptOverlapped 포인터</param>
	void ReleaseOverlapped(AcceptOverlapped* overlapped);

private:
	std::shared_ptr<log::Logger> _logger;
	std::shared_ptr<SocketOptionBuilder> _socketOptionBuilder;
	SocketHandle _listenSocket = InvalidSocket;
	HANDLE _iocpHandle = INVALID_HANDLE_VALUE;

	/// <summary>AcceptEx 함수 포인터. WSAIoctl로 획득.</summary>
	LPFN_ACCEPTEX _fnAcceptEx = nullptr;

	/// <summary>GetAcceptExSockAddrs 함수 포인터. WSAIoctl로 획득.</summary>
	LPFN_GETACCEPTEXSOCKADDRS _fnGetAcceptExSockAddrs = nullptr;

	mem::ObjectPool<AcceptOverlapped> _overlappedPool;
	AcceptCallback _acceptCallback;
};

}
