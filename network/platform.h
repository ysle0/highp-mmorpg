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
#pragma comment(lib, "mwssock.lib")
//#include <WS2tcpip.h>
#include <WinSock2.h>
#include <MSWSock.h>

#elif defined(PLATFORM_LINUX)
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#ifdef PLATFORM_LINUX
#include <sys/epoll.h>
#include <sys/eventfd.h>
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_BSD)
#include <sys/event.h>    // kqueue
#include <sys/time.h>
#endif
#endif

//=============================================================================
// 타입 정의
//=============================================================================
#ifdef PLATFORM_WINDOWS
using SocketHandle = SOCKET;
using SocketLength = int;
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
constexpr int SocketError = SOCKET_ERROR;
#else
using SocketHandle = int;
using SocketLength = socklen_t;
constexpr SocketHandle InvalidSocket = -1;
constexpr int SocketError = -1;
#endif

//using SocketHandle = SOCKET;
//constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
//constexpr int SocketErr = SOCKET_ERROR;
//
//inline int GetLastSocketError() {
//	return WSAGetLastError();
//}
//
//inline void CloseSocketHandle(SocketHandle sh) {
//	closesocket(sh);
//}
//
//#else
//
//#define PLATFORM_LINUX
//#include <sys/socket.h>
//
//#endif
