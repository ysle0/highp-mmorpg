#pragma once

#include "platform.h"
#include "EIoType.h"
#include <IocpError.h>
#include <Logger.hpp>
#include <Result.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace highp::network {

/// <summary>
/// IOCP 완료 이벤트 정보를 담는 구조체.
/// WorkerLoop()에서 GetQueuedCompletionStatus() 결과를 래핑하여 전달한다.
/// </summary>
struct CompletionEvent {
	/// <summary>I/O 작업 유형. EIoType 참조.</summary>
	EIoType ioType;

	/// <summary>소켓 연결 시 등록한 완료 키. 일반적으로 Client 포인터.</summary>
	void* completionKey;

	/// <summary>비동기 I/O에 사용된 OVERLAPPED 구조체 포인터. OverlappedExt로 캐스팅 가능.</summary>
	LPOVERLAPPED overlapped;

	/// <summary>전송된 바이트 수</summary>
	DWORD bytesTransferred;

	/// <summary>I/O 작업 성공 여부</summary>
	bool success;

	/// <summary>실패 시 에러 코드 (GetLastError 값)</summary>
	DWORD errorCode;
};

/// <summary>
/// IOCP 완료 이벤트 처리 콜백 함수 타입.
/// IoCompletionPort::SetCompletionHandler()로 등록한다.
/// </summary>
/// <remark>
/// CompletionEvent 는 sink argument 로 copy&move 는 호출측에서 결정
/// </remark>
using CompletionHandler = std::function<void(CompletionEvent)>;

/// <summary>
/// Windows I/O Completion Port를 관리하는 클래스.
/// IOCP 커널 객체 생성, Worker 스레드 풀 관리, 소켓 연결을 담당한다.
/// </summary>
/// <remarks>
/// 사용 순서:
/// 1. Initialize()로 IOCP 생성 및 Worker 스레드 시작
/// 2. SetCompletionHandler()로 완료 이벤트 콜백 등록
/// 3. AssociateSocket()으로 소켓을 IOCP에 연결
/// 4. Shutdown()으로 정리
/// </remarks>
class IoCompletionPort final {
public:
	/// <summary>IOCP 작업 결과 타입</summary>
	using Res = fn::Result<void, err::EIocpError>;

	/// <summary>
	/// IoCompletionPort 생성자.
	/// </summary>
	/// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
	/// <param name="handler">완료 이벤트 처리 콜백 (선택적). RAII 원칙에 따라 생성 시 설정 권장.</param>
	explicit IoCompletionPort(
		std::shared_ptr<log::Logger> logger,
		CompletionHandler handler = nullptr);

	/// <summary>
	/// 소멸자. Shutdown()을 호출하여 리소스 정리.
	/// </summary>
	~IoCompletionPort() noexcept;

	IoCompletionPort(const IoCompletionPort&) = delete;
	IoCompletionPort& operator=(const IoCompletionPort&) = delete;
	IoCompletionPort(IoCompletionPort&&) = delete;
	IoCompletionPort& operator=(IoCompletionPort&&) = delete;

	/// <summary>
	/// IOCP를 초기화하고 Worker 스레드를 시작한다.
	/// </summary>
	/// <param name="workerThreadCount">생성할 Worker 스레드 수. 일반적으로 CPU 코어 수 권장.</param>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// CreateIoCompletionPort()로 IOCP 핸들을 생성하고,
	/// std::jthread로 Worker 스레드를 생성하여 WorkerLoop() 실행.
	/// </remarks>
	Res Initialize(int workerThreadCount);

	/// <summary>
	/// IOCP를 종료하고 모든 Worker 스레드를 정리한다.
	/// </summary>
	/// <remarks>
	/// PostQueuedCompletionStatus()로 종료 신호를 전달하여
	/// GetQueuedCompletionStatus()에서 블로킹된 스레드들을 깨운다.
	/// </remarks>
	void Shutdown();

	/// <summary>
	/// 소켓을 IOCP에 연결한다.
	/// </summary>
	/// <param name="socket">IOCP에 연결할 소켓 핸들</param>
	/// <param name="completionKey">완료 이벤트 시 전달받을 키. 일반적으로 Client 포인터.</param>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// 이 함수 호출 후 해당 소켓의 비동기 I/O 완료가 IOCP로 통지된다.
	/// </remarks>
	Res AssociateSocket(SocketHandle socket, void* completionKey);

	/// <summary>
	/// 수동으로 완료 이벤트를 IOCP 큐에 추가한다.
	/// </summary>
	/// <param name="bytes">전송 바이트 수</param>
	/// <param name="key">완료 키</param>
	/// <param name="overlapped">OVERLAPPED 구조체 포인터</param>
	/// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
	/// <remarks>
	/// 주로 Worker 스레드 종료 신호 전달에 사용.
	/// </remarks>
	Res PostCompletion(DWORD bytes, void* key, LPOVERLAPPED overlapped);

	/// <summary>
	/// IOCP 완료 이벤트 처리 콜백을 등록한다.
	/// </summary>
	/// <param name="handler">완료 이벤트 발생 시 호출될 콜백 함수</param>
	void SetCompletionHandler(CompletionHandler handler);

	/// <summary>IOCP 커널 핸들을 반환한다. Acceptor::Initialize()에 전달용.</summary>
	/// <returns>IOCP 핸들</returns>
	HANDLE GetHandle() const noexcept { return _handle; }

	/// <summary>IOCP가 실행 중인지 확인한다.</summary>
	/// <returns>실행 중이면 true</returns>
	bool IsRunning() const noexcept { return _isRunning.load(); }

private:
	/// <summary>
	/// Worker 스레드에서 실행되는 메인 루프.
	/// GetQueuedCompletionStatus()로 완료 이벤트를 대기하고 콜백을 호출한다.
	/// </summary>
	/// <param name="st">스레드 중지 토큰 (std::jthread 제공)</param>
	void WorkerLoop(std::stop_token st);

private:
	/// <summary>로거 인스턴스</summary>
	std::shared_ptr<log::Logger> _logger;

	/// <summary>IOCP 커널 핸들</summary>
	HANDLE _handle = INVALID_HANDLE_VALUE;

	/// <summary>Worker 스레드 목록. std::jthread로 자동 join 및 취소 지원.</summary>
	std::vector<std::jthread> _workerThreads;

	/// <summary>IOCP 실행 상태 플래그</summary>
	std::atomic<bool> _isRunning{ false };

	/// <summary>완료 이벤트 처리 콜백</summary>
	CompletionHandler _completionHandler;
};

}
