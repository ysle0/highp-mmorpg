#pragma once

//=============================================================================
// 플랫폼 감지
//=============================================================================
#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define PLATFORM_MACOS
#endif
#define PLATFORM_UNIX
#elif defined(__linux__)
#define PLATFORM_LINUX
#define PLATFORM_UNIX
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define PLATFORM_BSD
#define PLATFORM_UNIX
#else
#error "Unsupported platform"
#endif

//=============================================================================
// 플랫폼별 헤더
//=============================================================================
#ifdef PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#include <MSWSock.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <mstcpip.h> // tcp_keepalive, SIO_KEEPALIVE_VALS

#elif defined(PLATFORM_LINUX)
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef PLATFORM_LINUX
#include <sys/epoll.h>
#include <sys/eventfd.h>
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_BSD)
#include <sys/event.h> // kqueue
#include <sys/time.h>
#endif

#endif

//=============================================================================
// 타입 정의
//=============================================================================
#ifdef PLATFORM_WINDOWS

/// <summary>플랫폼 독립적 소켓 핸들 타입. Windows에서는 SOCKET.</summary>
using SocketHandle = SOCKET;

/// <summary>소켓 주소 길이 타입. Windows에서는 int.</summary>
using SocketLength = int;

/// <summary>유효하지 않은 소켓 핸들 상수. Windows에서는
/// INVALID_SOCKET.</summary>
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;

/// <summary>소켓 에러 반환값 상수. Windows에서는 SOCKET_ERROR.</summary>
constexpr int SocketError = SOCKET_ERROR;

#else
using SocketHandle = int;
using SocketLength = int;
constexpr SocketHandle InvalidSocket = -1;
constexpr int SocketError = -1;
#endif
