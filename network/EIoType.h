#pragma once

namespace highp::network {

/// <summary>
/// 비동기 I/O 작업 유형을 구분하는 열거형.
/// OverlappedExt::ioType에 설정되어 CompletionEvent 처리 시 작업 종류를 식별한다.
/// IoCompletionPort::WorkerLoop()에서 분기 처리에 사용된다.
/// </summary>
enum class EIoType {
	/// <summary>AcceptEx 비동기 연결 수락 작업</summary>
	Accept,

	/// <summary>WSARecv 비동기 수신 작업</summary>
	Recv,

	/// <summary>WSASend 비동기 송신 작업</summary>
	Send,

	/// <summary>연결 해제 작업</summary>
	Disconnect
};

}
