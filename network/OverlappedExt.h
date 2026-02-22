#pragma once
#include "platform.h"
#include "EIoType.h"
#include "Const.h"

namespace highp::network {
    struct OverlappedBase {
        WSAOVERLAPPED overlapped;
        EIoType ioType;
    };

    /// <summary>
    /// 비동기 I/O 작업에 사용되는 확장 OVERLAPPED 구조체.
    /// WSAOVERLAPPED를 첫 번째 멤버로 배치하여 LPOVERLAPPED 캐스팅 안전성을 보장한다.
    /// IoCompletionPort::WorkerLoop() 및 Acceptor::PostAccept()에서 사용된다.
    /// </summary>
    /// <remarks>
    /// WSAOVERLAPPED가 반드시 첫 번째 멤버여야 하는 이유:
    /// - WSARecv, WSASend, AcceptEx 등에 LPOVERLAPPED로 전달
    /// - GetQueuedCompletionStatus()에서 반환된 LPOVERLAPPED를 OverlappedExt*로 캐스팅
    /// - 첫 번째 멤버가 아니면 메모리 레이아웃 불일치로 잘못된 주소 참조 발생
    /// </remarks>
    struct SendOverlapped {
        /// <summary>
        /// Windows 비동기 I/O용 OVERLAPPED 구조체.
        /// 반드시 구조체의 첫 번째 멤버로 위치해야 한다.
        /// </summary>
        /// <remarks>
        /// reinterpret_cast<SendOverlapped>(lpOverlapped) 가 올바르게 동작하려면
        /// 이 멤버가 오프셋 0에 위치해야 함.
        /// </remarks>
        WSAOVERLAPPED overlapped;
        /// <summary>I/O 작업 유형을 구분하는 플래그. EIoType 열거형 값.</summary>
        EIoType ioType = EIoType::Send;
        /// <summary>AcceptEx에서 사용되는 클라이언트 소켓 핸들</summary>
        SocketHandle clientSocket = InvalidSocket;
        /// <summary>WSARecv/WSASend에 전달되는 버퍼 디스크립터</summary>
        WSABUF bufDesc;
        /// <summary>실제 데이터 버퍼. </summary>
        char buf[Const::Buffer::sendBufferSize];
    };

    struct RecvOverlapped {
        WSAOVERLAPPED overlapped;
        EIoType ioType = EIoType::Recv;
        SocketHandle clientSocket = InvalidSocket;
        WSABUF bufDesc;
        char buf[Const::Buffer::recvBufferSize];
    };

    struct AcceptOverlapped {
        WSAOVERLAPPED overlapped;
        EIoType ioType = EIoType::Accept;
        SocketHandle clientSocket = InvalidSocket;
        WSABUF bufDesc;
        char buf[Const::Buffer::addressBufferSize];
    };
}
