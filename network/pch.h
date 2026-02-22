// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"

#include <error/Errors.hpp>
#include <macro/macro.h>
#include <functional/Result.hpp>
#include <error/NetworkError.h>
#include "client/windows/Client.h"
#include "config/Const.h"
#include "client/windows/EIoType.h"
#include "client/windows/OverlappedExt.h"
#include "transport/NetworkTransport.hpp"
#include "util/NetworkUtil.hpp"

#endif //PCH_H
