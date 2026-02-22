#pragma once

namespace highp::network {
    /// <summary>
    /// port 가 바른 범위에 있는지 확인 (unsigned short)
    /// </summary>
    /// <param name="port"></param>
    /// <returns></returns>
    inline bool IsValidPort(unsigned short port) {
        return port > 0 && port <= 65535;
    }
}
