#pragma once
#include <macro/macro.h>
#include <functional/Result.hpp>
#include <error/NetworkError.h>
#include "transport/NetworkTransport.hpp"

namespace highp::network {
    /// <summary>
    /// 소켓 추상화 인터페이스.
    /// TCP/UDP 등 전송 프로토콜에 관계없이 일관된 소켓 API를 제공한다.
    /// </summary>
    /// <see cref="WindowsAsyncSocket"/>
    class ISocket {
    public:
        /// <summary>소켓 작업 결과 타입</summary>
        using Res = fn::Result<void, err::ENetworkError>;

        /// <summary>가상 소멸자</summary>
        virtual ~ISocket() = default;

        /// <summary>
        /// Winsock을 초기화한다. WSAStartup() 호출.
        /// </summary>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        virtual Res Initialize() PURE;

        /// <summary>
        /// 지정된 전송 프로토콜로 소켓을 생성한다.
        /// </summary>
        /// <param name="transport">전송 프로토콜 (TCP/UDP)</param>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        /// <see cref="NetworkTransport"/>
        virtual Res CreateSocket(NetworkTransport transport) PURE;

        /// <summary>
        /// 소켓을 지정된 포트에 바인딩한다.
        /// </summary>
        /// <param name="port">바인딩할 포트 번호</param>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        virtual Res Bind(unsigned short port) PURE;

        /// <summary>
        /// 소켓을 Listen 상태로 전환한다.
        /// </summary>
        /// <param name="backlog">대기 연결 큐 크기</param>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        virtual Res Listen(int backlog) PURE;

        /// <summary>
        /// 소켓 리소스를 정리한다. closesocket() 및 WSACleanup() 호출.
        /// </summary>
        /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
        virtual Res Cleanup() PURE;

        /// <summary>
        /// 내부 소켓 핸들을 반환한다.
        /// </summary>
        /// <returns>소켓 핸들</returns>
        virtual SocketHandle GetSocketHandle() const PURE;
    };
}
