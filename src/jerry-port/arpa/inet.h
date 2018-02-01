// Copyright (c) 2018, Intel Corporation.

// Zephyr network stack port
// JerryScript debugger includes <arpa/inet.h> but Zephyr has different headers

#include <sys/fcntl.h>
#include <net/socket.h>

#define SOL_SOCKET    (1)
#define SO_REUSEADDR  (201)

#define setsockopt(sd, level, optname, optval, optlen) 0

char addr_str[NET_IPV4_ADDR_LEN];

#define socket(domain, type, protocol) socket(domain, type, IPPROTO_TCP)

inline char *inet_ntoa (struct in_addr addr) {
    return net_addr_ntop(AF_INET, &addr, addr_str, sizeof(addr_str));
}
