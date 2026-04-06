cmake_minimum_required(VERSION 4.1)
project(highp_mmorpg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HIGHP_FLATBUFFERS_HINTS
        "$ENV{FLATBUFFERS_ROOT}"
        "$ENV{flatbuffers_ROOT}"
        "$ENV{HOMEBREW_PREFIX}/opt/flatbuffers"
        "/opt/homebrew/opt/flatbuffers"
        "/usr/local/opt/flatbuffers")
list(FILTER HIGHP_FLATBUFFERS_HINTS EXCLUDE REGEX "^$")

find_package(flatbuffers CONFIG REQUIRED HINTS ${HIGHP_FLATBUFFERS_HINTS})

if (NOT TARGET flatbuffers::flatc)
    message(FATAL_ERROR "flatbuffers::flatc target was not found. Check your FlatBuffers installation.")
endif ()

file(GLOB_RECURSE HIGHP_FLATBUFFERS_SCHEMAS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/protocol/flatbuf/schemas/*.fbs")
list(SORT HIGHP_FLATBUFFERS_SCHEMAS)

set(HIGHP_FLATBUFFERS_GENERATED_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(HIGHP_FLATBUFFERS_GENERATED_DIR "${HIGHP_FLATBUFFERS_GENERATED_ROOT}/flatbuf/gen")
set(HIGHP_FLATBUFFERS_GENERATED_HEADERS)

foreach (schema IN LISTS HIGHP_FLATBUFFERS_SCHEMAS)
    get_filename_component(schemaName "${schema}" NAME_WE)
    list(APPEND HIGHP_FLATBUFFERS_GENERATED_HEADERS
            "${HIGHP_FLATBUFFERS_GENERATED_DIR}/${schemaName}_generated.h")
endforeach ()

add_custom_command(
        OUTPUT ${HIGHP_FLATBUFFERS_GENERATED_HEADERS}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${HIGHP_FLATBUFFERS_GENERATED_DIR}"
        COMMAND $<TARGET_FILE:flatbuffers::flatc>
        --cpp
        --scoped-enums
        -I "${CMAKE_CURRENT_SOURCE_DIR}/protocol/flatbuf/schemas"
        -o "${HIGHP_FLATBUFFERS_GENERATED_DIR}"
        ${HIGHP_FLATBUFFERS_SCHEMAS}
        DEPENDS flatbuffers::flatc ${HIGHP_FLATBUFFERS_SCHEMAS}
        COMMENT "Generating FlatBuffers headers"
        VERBATIM)

add_custom_target(highp_protocol_flatbuffers_codegen
        DEPENDS ${HIGHP_FLATBUFFERS_GENERATED_HEADERS})

include_directories(exec/chat/chat-client)
include_directories(exec/chat/chat-server/chat-server)
include_directories(exec/chat/chat-server/chat-server/handlers)
include_directories(exec/echo/echo-client)
include_directories(exec/echo/echo-server)
include_directories(lib)
include_directories(lib/inc)
include_directories(lib/inc/concurrency)
include_directories(lib/inc/config)
include_directories(lib/inc/config/compileTime)
include_directories(lib/inc/config/runTime)
include_directories(lib/inc/error)
include_directories(lib/inc/functional)
include_directories(lib/inc/logger)
include_directories(lib/inc/macro)
include_directories(lib/inc/memory)
include_directories(lib/inc/scope)
include_directories(network)
include_directories(network/inc)
include_directories(network/inc/acceptor)
include_directories(network/inc/client)
include_directories(network/inc/client/windows)
include_directories(network/inc/config)
include_directories(network/inc/io)
include_directories(network/inc/io/windows)
include_directories(network/inc/server)
include_directories(network/inc/socket)
include_directories(network/inc/transport)
include_directories(network/inc/util)
include_directories(protocol)

add_executable(highp_mmorpg
        exec/chat/chat-client/Client.cpp
        exec/chat/chat-client/Client.h
        exec/chat/chat-client/main.cpp
        exec/chat/chat-server/chat-server/handlers/ChatMessageHandler.cpp
        exec/chat/chat-server/chat-server/handlers/ChatMessageHandler.h
        exec/chat/chat-server/chat-server/handlers/JoinRoomHandler.cpp
        exec/chat/chat-server/chat-server/handlers/JoinRoomHandler.h
        exec/chat/chat-server/chat-server/main.cpp
        exec/chat/chat-server/chat-server/Server.cpp
        exec/chat/chat-server/chat-server/Server.h
        exec/echo/echo-client/EchoClient.cpp
        exec/echo/echo-client/EchoClient.h
        exec/echo/echo-client/main.cpp
        exec/echo/echo-server/main.cpp
        exec/echo/echo-server/Server.cpp
        exec/echo/echo-server/Server.h
        exec/playground/playground.cpp
        lib/inc/concurrency/MPSCCommandQueue.hpp
        lib/inc/config/compileTime/Config.hpp
        lib/inc/config/compileTime/StringUtils.hpp
        lib/inc/config/compileTime/TomlEntry.hpp
        lib/inc/config/compileTime/TomlParser.hpp
        lib/inc/config/runTime/Config.hpp
        lib/inc/config/runTime/StringUtils.hpp
        lib/inc/config/runTime/TomlEntry.hpp
        lib/inc/config/runTime/TomlParser.hpp
        lib/inc/error/Errors.hpp
        lib/inc/error/NetworkError.h
        lib/inc/error/PacketError.h
        lib/inc/functional/Result.hpp
        lib/inc/logger/ELoggerVerbosity.h
        lib/inc/logger/ILogger.h
        lib/inc/logger/Logger.hpp
        lib/inc/logger/LoggerOptions.h
        lib/inc/logger/TextLogger.h
        lib/inc/macro/macro.h
        lib/inc/memory/HybridObjectPool.hpp
        lib/inc/memory/LockObjectPool.hpp
        lib/inc/memory/ThreadLocalPool.hpp
        lib/inc/scope/Defer.h
        lib/inc/scope/DeferContext.hpp
        lib/inc/scope/DeferredItemHolder.hpp
        lib/inc/thread/LogicThread.cpp
        lib/inc/thread/LogicThread.h
        lib/src/logger/TextLogger.cpp
        lib/src/scope/Defer.cpp
        lib/framework.h
        lib/lib.cpp
        lib/pch.cpp
        lib/pch.h
        network/inc/acceptor/AcceptContext.h
        network/inc/acceptor/IocpAcceptor.h
        network/inc/client/windows/Client.h
        network/inc/client/windows/EIoType.h
        network/inc/client/windows/OverlappedExt.h
        network/inc/client/FrameBuffer.h
        network/inc/client/PacketStream.h
        network/inc/client/TcpClientSocket.h
        network/inc/client/WsaSession.h
        network/inc/config/ConfigEmbedded.h
        network/inc/config/Const.h
        network/inc/config/NetworkCfg.h
        network/inc/io/windows/IocpIoMultiplexer.h
        network/inc/io/CompletionTarget.hpp
        network/inc/server/IPacketHandler.hpp
        network/inc/server/ISessionEventReceiver.h
        network/inc/server/PacketDispatcher.hpp
        network/inc/server/ServerLifecycle.h
        network/inc/socket/AsyncSocket.h
        network/inc/socket/ISocket.h
        network/inc/socket/SocketHelper.h
        network/inc/socket/SocketOptionBuilder.h
        network/inc/socket/WindowsAsyncSocket.h
        network/inc/transport/NetworkTransport.hpp
        network/inc/util/NetworkUtil.hpp
        network/inc/platform.h
        network/src/acceptor/IocpAcceptor.cpp
        network/src/client/windows/Client.cpp
        network/src/client/FrameBuffer.cpp
        network/src/client/PacketStream.cpp
        network/src/client/TcpClientSocket.cpp
        network/src/client/WsaSession.cpp
        network/src/io/windows/IocpIoMultiplexer.cpp
        network/src/server/PacketDispatcher.cpp
        network/src/socket/AsyncSocket.cpp
        network/src/socket/SocketHelper.cpp
        network/src/socket/SocketOptionBuilder.cpp
        network/src/socket/WindowsAsyncSocket.cpp
        network/src/ServerLifecycle.cpp
        network/framework.h
        network/network.cpp
        network/pch.cpp
        network/pch.h
        protocol/framework.h
        protocol/pch.cpp
        protocol/pch.h
        protocol/protocol.cpp)

add_dependencies(highp_mmorpg highp_protocol_flatbuffers_codegen)

target_include_directories(highp_mmorpg PRIVATE
        "${HIGHP_FLATBUFFERS_GENERATED_ROOT}")

target_link_libraries(highp_mmorpg PRIVATE
        flatbuffers::flatbuffers)

target_compile_options(highp_mmorpg PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang>:-fexperimental-library>)
