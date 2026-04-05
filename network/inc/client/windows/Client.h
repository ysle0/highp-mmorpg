#pragma once

#include <functional>
#include <string_view>

#include "platform.h"
#include "OverlappedExt.h"
#include <error/NetworkError.h>
#include <functional/Result.hpp>
#include <memory>

#include "client/FrameBuffer.h"
#include "io/CompletionTarget.hpp"

namespace highp::net {
    /// <summary>
    /// 클라이언트 연결 상태 및 I/O 버퍼를 관리하는 구조체.
    /// std::enable_shared_from_this를 상속하여 콜백에서 안전하게 shared_ptr 획득 가능.
    /// </summary>
    /// <remarks>
    /// 각 클라이언트마다 Recv/Send용 OverlappedExt를 보유하여 동시 I/O 작업을 지원한다.
    /// </remarks>
    struct Client : std::enable_shared_from_this<Client>, ICompletionTarget {
        using Res = fn::Result<void, err::ENetworkError>;

        struct EventCallbacks {
            std::function<void()> onConnected;
            std::function<void()> onDisconnected;
            std::function<void()> onRecvPosted;
            std::function<void()> onRecvPostFailed;
            std::function<void()> onSendPosted;
            std::function<void()> onSendPostFailed;
        };

        /// <summary>기본 생성자. 소켓을 INVALID_SOCKET으로 초기화.</summary>
        Client();

        ~Client() noexcept override;

        /// <summary>
        /// 비동기 수신을 시작한다.
        /// </summary>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        Res PostRecv();

        /// <summary>
        /// 비동기 송신을 수행한다.
        /// </summary>
        /// <param name="data">송신할 데이터</param>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        Res PostSend(std::string_view data);

        /// <summary>
        /// 클라이언트 연결을 종료하고 소켓을 닫는다.
        /// </summary>
        /// <param name="isFireAndForget">true면 linger 없이 즉시 종료</param>
        void Close(bool isFireAndForget);

        /// <summary>이벤트 콜백을 연결한다.</summary>
        void UseCallbacks(EventCallbacks callbacks) noexcept;

        /// <summary>이 연결이 실제로 수락되었음을 표시한다.</summary>
        void MarkConnectionEstablished() noexcept;

        /// <summary>연결 성립 이벤트를 발생시킨다.</summary>
        void EmitConnectedEvent();

        /// <summary>연결 종료 이벤트를 발생시킨다.</summary>
        void EmitDisconnectedEvent();

        /// <summary>Recv 요청이 게시되었음을 알린다.</summary>
        void EmitRecvPosted();

        /// <summary>Recv 요청 게시 실패를 알린다.</summary>
        void EmitRecvPostFailed();

        /// <summary>Send 요청이 게시되었음을 알린다.</summary>
        void EmitSendPosted();

        /// <summary>Send 요청 게시 실패를 알린다.</summary>
        void EmitSendPostFailed();

        /// <summary>프레임 조립용 누적 버퍼</summary>
        FrameBuffer& FrameBuf() { return _frameBuf; }

        bool IsConnected() const noexcept { return socket != INVALID_SOCKET; }

        /// <summary>클라이언트 소켓 핸들</summary>
        SocketHandle socket = INVALID_SOCKET;

        /// <summary>수신 작업용 SendOverlapped 구조체</summary>
        internal::RecvOverlapped recvOverlapped;

        /// <summary>송신 작업용 SendOverlapped 구조체</summary>
        internal::SendOverlapped sendOverlapped;

    private:
        FrameBuffer _frameBuf;
        EventCallbacks _callbacks;
        bool _connectionEstablished = false;
        bool _connectionEventCounted = false;
    };
}
