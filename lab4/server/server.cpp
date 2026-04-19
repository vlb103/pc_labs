#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <memory>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include "../shared/protocol.h"

using namespace std;

uint64_t custom_htonll(uint64_t value) {
    if (htonl(1) == 1) return value;
    return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
}

uint64_t custom_ntohll(uint64_t value) {
    if (ntohl(1) == 1) return value;
    return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
}

void CalculateSumRange(const vector<int32_t>& matrix, size_t start_idx, size_t end_idx, long long& result_ref) {
    long long sum = 0;
    for (size_t i = start_idx; i < end_idx; ++i) {
        sum += matrix[i];
    }
    result_ref = sum;
}

long long CalculateParallelSum(const vector<int32_t>& matrix, int num_threads) {
    size_t total = matrix.size();
    if (total == 0) return 0;

    if (num_threads > static_cast<int>(total)) {
        num_threads = static_cast<int>(total);
    }
    if (num_threads < 1) {
        num_threads = 1;
    }

    vector<thread> threads(num_threads);
    vector<long long> partial_results(num_threads, 0);

    size_t chunk_size = total / num_threads;
    size_t remainder = total % num_threads;

    size_t start = 0;
    for (int i = 0; i < num_threads; ++i) {
        size_t end = start + chunk_size + (i < static_cast<int>(remainder) ? 1 : 0);
        threads[i] = thread(CalculateSumRange, cref(matrix), start, end, ref(partial_results[i]));
        start = end;
    }

    for (auto& t : threads) {
        t.join();
    }

    long long total_sum = 0;
    for (int i = 0; i < num_threads; ++i) {
        total_sum += partial_results[i];
    }
    return total_sum;
}

bool recv_exact(SOCKET sock, char* buf, int len) {
    int received = 0;
    while (received < len) {
        int r = recv(sock, buf + received, len - received, 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

bool send_exact(SOCKET sock, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int s = send(sock, buf + sent, len - sent, 0);
        if (s <= 0) return false;
        sent += s;
    }
    return true;
}

struct ClientTaskContext {
    atomic<StatusType> status{StatusType::IDLE};
    atomic<long long> final_result{0};
    vector<int32_t> matrix;
};

void handle_client(SOCKET client_sock) {
    cout << "[SERVER] client connected.\n";

    uint32_t array_size = 0;
    uint32_t thread_count = 0;
    bool configured = false;
    auto ctx = make_shared<ClientTaskContext>();

    while (true) {
        MessageHeader header;
        if (!recv_exact(client_sock, reinterpret_cast<char*>(&header), sizeof(header))) {
            cout << "[SERVER] client disconnected.\n";
            break;
        }

        header.payload_size = ntohl(header.payload_size);

        switch (header.command) {

        case CommandType::SEND_CONFIG: {
            if (ctx->status.load() == StatusType::IN_PROGRESS) {
                cerr << "[SERVER] CONFIG rejected: calculation in progress.\n";
                break;
            }

            ConfigPayload cfg;
            if (!recv_exact(client_sock, reinterpret_cast<char*>(&cfg), sizeof(cfg))) {
                cout << "[SERVER] client disconnected during config payload.\n";
                shutdown(client_sock, SD_BOTH);
                closesocket(client_sock);
                return;
            }

            array_size = ntohl(cfg.array_size);
            thread_count = ntohl(cfg.thread_count);

            if (array_size > 100000000) {
                cerr << "[SERVER] array_size too large, rejecting client.\n";
                shutdown(client_sock, SD_BOTH);
                closesocket(client_sock);
                return;
            }

            uint32_t expected_payload = static_cast<uint32_t>(sizeof(ConfigPayload) + array_size * sizeof(int32_t));
            if (header.payload_size != expected_payload) {
                cerr << "[SERVER] payload_size mismatch: got " << header.payload_size
                     << ", expected " << expected_payload << ". Dropping client.\n";
                shutdown(client_sock, SD_BOTH);
                closesocket(client_sock);
                return;
            }

            cout << "[SERVER] received CONFIG: array_size=" << array_size
                 << ", thread_count=" << thread_count << "\n";

            try {
                ctx->matrix.resize(array_size);
            } catch (const bad_alloc&) {
                cerr << "[SERVER] failed to allocate memory, rejecting client.\n";
                shutdown(client_sock, SD_BOTH);
                closesocket(client_sock);
                return;
            }

            int bytes_to_read = static_cast<int>(array_size * sizeof(int32_t));

            cout << "[SERVER] receiving array data (" << bytes_to_read << " bytes)...\n";
            if (!recv_exact(client_sock, reinterpret_cast<char*>(ctx->matrix.data()), bytes_to_read)) {
                cerr << "[SERVER] failed to receive array data.\n";
                shutdown(client_sock, SD_BOTH);
                closesocket(client_sock);
                return;
            }

            for (uint32_t i = 0; i < array_size; ++i) {
                ctx->matrix[i] = ntohl(ctx->matrix[i]);
            }

            cout << "[SERVER] array data received successfully.\n";

            configured = true;
            break;
        }

        case CommandType::START_PROCESSING: {
            if (header.payload_size > 0) {
                cerr << "[SERVER] START_PROCESSING has unexpected payload, dropping client.\n";
                break;
            }
            if (!configured) {
                cerr << "[SERVER] START received before CONFIG, ignoring.\n";
                break;
            }
            if (ctx->status.load() != StatusType::IDLE) {
                cerr << "[SERVER] START ignored: not in IDLE state.\n";
                break;
            }
            cout << "[SERVER] START_PROCESSING: launching parallel sum...\n";
            ctx->status.store(StatusType::IN_PROGRESS);

            thread([ctx, thread_count]() {
                ctx->final_result.store(CalculateParallelSum(ctx->matrix, thread_count));
                ctx->status.store(StatusType::READY);
                cout << "[SERVER] calculation finished. Result = "
                     << ctx->final_result.load() << "\n";
            }).detach();

            MessageHeader ack;
            ack.command = CommandType::START_PROCESSING;
            ack.payload_size = htonl(0);
            if (!send_exact(client_sock, reinterpret_cast<char*>(&ack), sizeof(ack))) {
                cerr << "[SERVER] failed to send START ack.\n";
                break;
            }
            cout << "[SERVER] sent START_PROCESSING ack.\n";

            break;
        }

        case CommandType::GET_STATUS: {
            if (header.payload_size > 0) {
                cerr << "[SERVER] GET_STATUS has unexpected payload, dropping client.\n";
                break;
            }
            StatusResponse resp;
            resp.status = ctx->status.load();

            MessageHeader resp_hdr;
            resp_hdr.command = CommandType::GET_STATUS;
            resp_hdr.payload_size = htonl(sizeof(StatusResponse));

            if (!send_exact(client_sock, reinterpret_cast<char*>(&resp_hdr), sizeof(resp_hdr)) ||
                !send_exact(client_sock, reinterpret_cast<char*>(&resp), sizeof(resp))) {
                cerr << "[SERVER] failed to send STATUS.\n";
                break;
            }

            cout << "[SERVER] sent STATUS: "
                 << (ctx->status.load() == StatusType::READY ? "READY" : "IN_PROGRESS")
                 << "\n";
            break;
        }

        case CommandType::GET_RESULT: {
            if (header.payload_size > 0) {
                cerr << "[SERVER] GET_RESULT has unexpected payload, dropping client.\n";
                break;
            }
            ResultResponse resp;
            resp.final_result = (int64_t)custom_htonll((uint64_t)ctx->final_result.load());

            MessageHeader resp_hdr;
            resp_hdr.command = CommandType::GET_RESULT;
            resp_hdr.payload_size = htonl(sizeof(ResultResponse));

            if (!send_exact(client_sock, reinterpret_cast<char*>(&resp_hdr), sizeof(resp_hdr)) ||
                !send_exact(client_sock, reinterpret_cast<char*>(&resp), sizeof(resp))) {
                cerr << "[SERVER] failed to send RESULT.\n";
                break;
            }

            cout << "[SERVER] sent RESULT: " << ctx->final_result.load() << "\n";
            break;
        }

        default:
            cerr << "[SERVER] unknown command: "
                 << static_cast<int>(header.command) << "\n";
            break;
        }
    }

    shutdown(client_sock, SD_BOTH);
    closesocket(client_sock);
    cout << "[SERVER] client socket closed.\n";
}

int main() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        cerr << "[SERVER] WSAStartup failed.\n";
        return 1;
    }
    cout << "[SERVER] WinSock initialized.\n";

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        cerr << "[SERVER] socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (bind(listen_sock, reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "[SERVER] bind() failed: " << WSAGetLastError() << "\n";
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    cout << "[SERVER] bound to 127.0.0.1:8080\n";

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[SERVER] listen() failed: " << WSAGetLastError() << "\n";
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    cout << "[SERVER] listening for connections...\n";

    while (true) {
        SOCKET client_sock = accept(listen_sock, nullptr, nullptr);
        if (client_sock == INVALID_SOCKET) {
            cerr << "[SERVER] accept() failed: " << WSAGetLastError() << "\n";
            continue;
        }
        thread(handle_client, client_sock).detach();
    }

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
