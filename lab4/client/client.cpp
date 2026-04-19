#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include "../shared/protocol.h"

using namespace std;

uint64_t custom_ntohll(uint64_t value) {
    if (ntohl(1) == 1) return value;
    return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
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

int main() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        cerr << "[CLIENT] WSAStartup failed.\n";
        return 1;
    }
    cout << "[CLIENT] WinSock initialized.\n";

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "[CLIENT] socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    cout << "[CLIENT] connecting to 127.0.0.1:8080...\n";
    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "[CLIENT] connect() failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    cout << "[CLIENT] connected to server.\n";

    {
        const uint32_t ARRAY_SIZE = 1000;
        const uint32_t THREAD_COUNT = 4;

        vector<int32_t> matrix(ARRAY_SIZE);
        srand(static_cast<unsigned>(time(nullptr)));
        long long local_sum = 0;
        for (uint32_t i = 0; i < ARRAY_SIZE; ++i) {
            matrix[i] = rand() % 10 + 1;
            local_sum += matrix[i];
        }
        cout << "[CLIENT] generated random array (" << ARRAY_SIZE
             << " elements). local checksum = " << local_sum << "\n";

        MessageHeader hdr;
        hdr.command = CommandType::SEND_CONFIG;
        hdr.payload_size = htonl(sizeof(ConfigPayload) + ARRAY_SIZE * sizeof(int32_t));

        ConfigPayload cfg;
        cfg.array_size = htonl(ARRAY_SIZE);
        cfg.thread_count = htonl(THREAD_COUNT);

        send_exact(sock, reinterpret_cast<char*>(&hdr), sizeof(hdr));
        send_exact(sock, reinterpret_cast<char*>(&cfg), sizeof(cfg));

        vector<int32_t> net_matrix(ARRAY_SIZE);
        for (uint32_t i = 0; i < ARRAY_SIZE; ++i) {
            net_matrix[i] = htonl(matrix[i]);
        }

        int array_bytes = static_cast<int>(ARRAY_SIZE * sizeof(int32_t));
        send_exact(sock, reinterpret_cast<const char*>(net_matrix.data()), array_bytes);

        cout << "[CLIENT] sent CONFIG: array_size=" << ARRAY_SIZE
             << ", thread_count=" << THREAD_COUNT << "\n";
        cout << "[CLIENT] sent array data (" << array_bytes << " bytes)\n";
    }

    {
        MessageHeader hdr;
        hdr.command = CommandType::START_PROCESSING;
        hdr.payload_size = htonl(0);

        send_exact(sock, reinterpret_cast<char*>(&hdr), sizeof(hdr));

        cout << "[CLIENT] sent START_PROCESSING\n";

        MessageHeader ack;
        if (!recv_exact(sock, reinterpret_cast<char*>(&ack), sizeof(ack))) {
            cerr << "[CLIENT] lost connection while waiting for START ack.\n";
            closesocket(sock);
            WSACleanup();
            return 1;
        }
        cout << "[CLIENT] received START_PROCESSING ack from server.\n";
    }

    {
        cout << "[CLIENT] polling server status...\n";

        while (true) {
            MessageHeader req;
            req.command = CommandType::GET_STATUS;
            req.payload_size = htonl(0);
            send_exact(sock, reinterpret_cast<char*>(&req), sizeof(req));

            MessageHeader resp_hdr;
            if (!recv_exact(sock, reinterpret_cast<char*>(&resp_hdr), sizeof(resp_hdr))) {
                cerr << "[CLIENT] lost connection while waiting for status.\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            resp_hdr.payload_size = ntohl(resp_hdr.payload_size);

            StatusResponse resp;
            if (!recv_exact(sock, reinterpret_cast<char*>(&resp), sizeof(resp))) {
                cerr << "[CLIENT] lost connection while reading status payload.\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }

            if (resp.status == StatusType::READY) {
                cout << "[CLIENT] server status: READY\n";
                break;
            }

            cout << "[CLIENT] server is still calculating... waiting 50ms\n";
            this_thread::sleep_for(chrono::milliseconds(50));
        }
    }

    {
        MessageHeader req;
        req.command = CommandType::GET_RESULT;
        req.payload_size = htonl(0);
        send_exact(sock, reinterpret_cast<char*>(&req), sizeof(req));

        MessageHeader resp_hdr;
        if (!recv_exact(sock, reinterpret_cast<char*>(&resp_hdr), sizeof(resp_hdr))) {
            cerr << "[CLIENT] lost connection while waiting for result.\n";
            closesocket(sock);
            WSACleanup();
            return 1;
        }
        resp_hdr.payload_size = ntohl(resp_hdr.payload_size);

        ResultResponse resp;
        if (!recv_exact(sock, reinterpret_cast<char*>(&resp), sizeof(resp))) {
            cerr << "[CLIENT] lost connection while reading result payload.\n";
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        resp.final_result = (int64_t)custom_ntohll((uint64_t)resp.final_result);

        cout << "[CLIENT] final Result: " << resp.final_result << "\n";
    }

    closesocket(sock);
    WSACleanup();
    cout << "[CLIENT] done. connection closed.\n";

    return 0;
}
