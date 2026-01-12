#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <stop_token>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>
