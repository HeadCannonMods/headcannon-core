#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

namespace cameraunlock {

/// Checks whether a sockaddr_in represents a remote (non-loopback) address.
inline bool IsRemoteAddress(const sockaddr_in& addr) {
    return ntohl(addr.sin_addr.s_addr) != INADDR_LOOPBACK;
}

}  // namespace cameraunlock
