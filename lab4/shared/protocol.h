#pragma once
#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#pragma pack(push, 1)

enum class CommandType : uint8_t {
    SEND_CONFIG = 1,
    START_PROCESSING = 2,
    GET_STATUS = 3,
    GET_RESULT = 4
};

enum class StatusType : uint8_t {
    IN_PROGRESS = 0,
    READY = 1,
    IDLE = 2,
    STATUS_ERROR = 255
};

struct MessageHeader {
    CommandType command;
    uint32_t payload_size;
};

struct ConfigPayload {
    uint32_t array_size;
    uint32_t thread_count;
};

struct StatusResponse {
    StatusType status;
};

struct ResultResponse {
    int64_t final_result;
};

#pragma pack(pop)

inline uint64_t custom_ntohll(uint64_t value) {
    if (ntohl(1) == 1) return value;
    return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
}

inline uint64_t custom_htonll(uint64_t value) {
    if (htonl(1) == 1) return value;
    return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
}

inline bool recv_exact(SOCKET sock, char* buf, int len) {
    int received = 0;
    while (received < len) {
        int r = recv(sock, buf + received, len - received, 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

inline bool send_exact(SOCKET sock, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int s = send(sock, buf + sent, len - sent, 0);
        if (s <= 0) return false;
        sent += s;
    }
    return true;
}
