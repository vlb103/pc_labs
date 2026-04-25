#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define PORT 8080

void sendResponse(SOCKET clientSocket, const string& response) {
    send(clientSocket, response.c_str(), response.size(), 0);
}

void handleRequest(SOCKET clientSocket) {
    char buffer[4096] = { 0 };
    recv(clientSocket, buffer, 4096, 0);
    string request(buffer);

    string method, path, protocol;
    istringstream requestStream(request);
    requestStream >> method >> path >> protocol;

    string response;
    if (method == "GET") {
        string filename = "";

        if (path == "/" || path == "/index.html") {
            filename = "index.html";
        }
        else if (path == "/page2.html" || path == "/second_page.html") {
            filename = "page2.html";
        }

        if (!filename.empty()) {
            ifstream file(filename);
            if (file.is_open()) {
                stringstream fileBuffer;
                fileBuffer << file.rdbuf();
                string content = fileBuffer.str();
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + to_string(content.size()) + "\r\n\r\n" + content;
            }
            else {
                string notFound = "<html><body><h1>404 File Missing</h1></body></html>";
                response = "HTTP/1.1 404 Not Found\r\nContent-Length: " + to_string(notFound.size()) + "\r\n\r\n" + notFound;
            }
        }
        else {
            string notFound = "<html><body><h1>404 Not Found</h1></body></html>";
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: " + to_string(notFound.size()) + "\r\n\r\n" + notFound;
        }
    }
    sendResponse(clientSocket, response);
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server listening on port " << PORT << "...\n";

    while (true) {
        struct sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket == INVALID_SOCKET) {
            cerr << "Accept failed\n";
            continue;
        }

        thread(handleRequest, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}