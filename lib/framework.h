#pragma once

#include <atomic>
#include <memory>
#include <stop_token>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
