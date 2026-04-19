#pragma once
#include <stdint.h>

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
