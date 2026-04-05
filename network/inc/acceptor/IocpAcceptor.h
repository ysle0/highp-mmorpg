#pragma once

#include "platform.h"
#include "AcceptContext.h"
#include "client/windows/Client.h"
#include "client/windows/OverlappedExt.h"
#include <logger/Logger.hpp>
#include <functional/Result.hpp>
#include <error/NetworkError.h>
#include <functional>
#include <memory>
#include "scope/DeferredItemHolder.hpp"

namespace highp::net {
    class SocketOptionBuilder;
}

namespace highp::net::internal {
    /// <summary>
    /// AcceptEx 기반 비동기 연결 수락을 담당하는 클래스.
    /// WSAIoctl로 AcceptEx 함수 포인터를 획득하여 간접 호출 방식을 사용한다.
    /// </summary>
    /// <remarks>
    /// 사용 순서:
    /// 1. Initialize()로 Listen 소켓을 IOCP에 연결하고 AcceptEx 함수 로드
    /// 3. PostAccepts()로 비동기 Accept 요청
    /// 4. IOCP Worker에서 Accept 완료 시 OnAcceptComplete() 호출
    /// 5. Shutdown()으로 정리
    /// </remarks>
    class IocpAcceptor final {
        /// <summary>
        /// Accept 완료 시 호출되는 콜백 함수 타입.
        /// </summary>
        using AcceptCallback = std::function<void(AcceptContext&)>;

    public:
        struct EventCallbacks {
            std::function<void()> onAcceptPosted;
            std::function<void()> onAcceptPostFailed;
            std::function<void()> onAcceptCompleted;
            std::function<void()> onAcceptCompletionFailed;
        };

    public:
        /// <summary>IocpAcceptor 작업 결과 타입</summary>
        using Res = fn::Result<void, err::ENetworkError>;

        /// <summary>
        /// IocpAcceptor 생성자.
        /// </summary>
        /// <param name="logger">로깅에 사용할 Logger 인스턴스</param>
        /// <param name="socketOptionBuilder">소켓 옵션 설정을 위한 빌더 (선택적)</param>
        /// <param name="onAfterAccept">Accept 완료 콜백 (선택적). RAII 원칙에 따라 생성 시 설정 권장.</param>
        explicit IocpAcceptor(
            std::shared_ptr<log::Logger> logger,
            std::shared_ptr<SocketOptionBuilder> socketOptionBuilder = nullptr,
            AcceptCallback onAfterAccept = nullptr);

        /// <summary>
        /// 소멸자. Shutdown()을 호출하여 리소스 정리.
        /// </summary>
        ~IocpAcceptor() noexcept;

        IocpAcceptor(const IocpAcceptor&) = delete;
        IocpAcceptor& operator=(const IocpAcceptor&) = delete;
        IocpAcceptor(IocpAcceptor&&) = delete;
        IocpAcceptor& operator=(IocpAcceptor&&) = delete;

        /// <summary>
        /// IocpAcceptor를 초기화한다.
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
        /// IocpAcceptor를 종료하고 리소스를 정리한다.
        /// </summary>
        void Shutdown();

        /// <summary>
        /// 이벤트 콜백을 연결한다.
        /// </summary>
        void UseCallbacks(EventCallbacks callbacks) noexcept;

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

        std::shared_ptr<log::Logger> _logger;
        std::shared_ptr<SocketOptionBuilder> _socketOptionBuilder;
        SocketHandle _listenSocket = InvalidSocket;
        HANDLE _iocpHandle = INVALID_HANDLE_VALUE;

        /// <summary>AcceptEx 함수 포인터. WSAIoctl로 획득.</summary>
        LPFN_ACCEPTEX _fnAcceptEx = nullptr;

        /// <summary>GetAcceptExSockAddrs 함수 포인터. WSAIoctl로 획득.</summary>
        LPFN_GETACCEPTEXSOCKADDRS _fnGetAcceptExSockAddrs = nullptr;

        EventCallbacks _callbacks;
        AcceptCallback _acceptCallback;

        scope::DeferredItemHolder<AcceptOverlapped> _acceptOverlappedHolder;
    };
}
