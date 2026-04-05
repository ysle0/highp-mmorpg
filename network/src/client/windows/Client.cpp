#include "pch.h"

#include "client/windows/Client.h"

namespace highp::net {
    Client::Client() {
        ZeroMemory(&recvOverlapped, sizeof(internal::RecvOverlapped));
        ZeroMemory(&sendOverlapped, sizeof(internal::SendOverlapped));
    }

    Client::~Client() noexcept {
        Close(true);
    }

    void Client::UseCallbacks(EventCallbacks callbacks) noexcept {
        _callbacks = std::move(callbacks);
    }

    void Client::MarkConnectionEstablished() noexcept {
        _connectionEstablished = true;
        EmitConnectedEvent();
    }

    void Client::EmitConnectedEvent() {
        if (!_connectionEstablished || _connectionEventCounted) {
            return;
        }

        _connectionEventCounted = true;
        if (_callbacks.onConnected) {
            _callbacks.onConnected();
        }
    }

    void Client::EmitDisconnectedEvent() {
        if (!_connectionEstablished || !_connectionEventCounted) {
            _connectionEstablished = false;
            _connectionEventCounted = false;
            return;
        }

        _connectionEventCounted = false;
        _connectionEstablished = false;
        if (_callbacks.onDisconnected) {
            _callbacks.onDisconnected();
        }
    }

    void Client::EmitRecvPosted() {
        if (_callbacks.onRecvPosted) {
            _callbacks.onRecvPosted();
        }
    }

    void Client::EmitRecvPostFailed() {
        if (_callbacks.onRecvPostFailed) {
            _callbacks.onRecvPostFailed();
        }
    }

    void Client::EmitSendPosted() {
        if (_callbacks.onSendPosted) {
            _callbacks.onSendPosted();
        }
    }

    void Client::EmitSendPostFailed() {
        if (_callbacks.onSendPostFailed) {
            _callbacks.onSendPostFailed();
        }
    }

    Client::Res Client::PostRecv() {
        ZeroMemory(&recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));
        recvOverlapped.bufDesc.buf = recvOverlapped.buf;
        recvOverlapped.bufDesc.len = std::size(recvOverlapped.buf);
        recvOverlapped.ioType = internal::EIoType::Recv;

        DWORD flags = 0;
        DWORD recvNumBytes = 0;

        const int result = WSARecv(
            socket,
            &recvOverlapped.bufDesc,
            1,
            &recvNumBytes,
            &flags,
            reinterpret_cast<LPWSAOVERLAPPED>(&recvOverlapped),
            nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
            EmitRecvPostFailed();
            return Res::Err(err::ENetworkError::WsaRecvFailed);
        }

        EmitRecvPosted();
        return Res::Ok();
    }

    Client::Res Client::PostSend(std::string_view data) {
        ZeroMemory(&sendOverlapped.overlapped, sizeof(WSAOVERLAPPED));

        const auto len = static_cast<ULONG>(data.size());
        CopyMemory(sendOverlapped.buf, data.data(), len);
        sendOverlapped.bufDesc.buf = sendOverlapped.buf;
        sendOverlapped.bufDesc.len = len;
        sendOverlapped.ioType = internal::EIoType::Send;

        DWORD sendNumBytes = 0;

        const int result = WSASend(
            socket,
            &sendOverlapped.bufDesc,
            1,
            &sendNumBytes,
            0,
            reinterpret_cast<LPWSAOVERLAPPED>(&sendOverlapped),
            nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
            EmitSendPostFailed();
            return Res::Err(err::ENetworkError::WsaSendFailed);
        }

        EmitSendPosted();
        return Res::Ok();
    }

    void Client::Close(bool isFireAndForget) {
        (void)isFireAndForget;

        EmitDisconnectedEvent();

        if (socket == INVALID_SOCKET) {
            return;
        }

        // linger linger{ 0, 0 };
        // if (isFireAndForget) {
        //	linger.l_onoff = 1;
        // }

        // setsockopt(socket,
        //	SOL_SOCKET,
        //	SO_LINGER,
        //	(char*)&linger,
        //	sizeof(linger));

        shutdown(socket, SD_BOTH);
        closesocket(socket);

        socket = INVALID_SOCKET;
    }
} // namespace highp::net
