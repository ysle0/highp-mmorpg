#include "pch.h"

#include "client/FrameBuffer.h"
#include "client/PacketStream.h"

namespace highp::net {
    bool FrameBuffer::Append(std::span<const char> data) {
        const size_t dataLen = data.size();
        if (bool isExceedBufferLen = _len + dataLen > _buf.size();
            isExceedBufferLen
        ) {
            return false;
        }

        // buffer 에 _len(offset) 부터 data 를 data.size() 만큼 복사.
        std::memcpy(_buf.data() + _len, data.data(), dataLen);

        _len += dataLen;
        return true;
    }

    std::span<const char> FrameBuffer::Data() const {
        return {_buf.data(), _len};
    }

    void FrameBuffer::Consume(size_t bytesTransferred) {
        if (bytesTransferred >= _len) {
            _len = 0;
            return;
        }
        
        std::memmove(
            _buf.data(),
            _buf.data() + bytesTransferred,
            _len - bytesTransferred);
        
        _len -= bytesTransferred;
    }

    size_t FrameBuffer::Size() const { return _len; }
} // namespace highp::net
