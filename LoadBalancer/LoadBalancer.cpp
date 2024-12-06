#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <ws2tcpip.h> // Za InetPton()

#pragma comment(lib, "ws2_32.lib")



#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 5059
#define BUFFER_SIZE 256

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    int bytesReceived;

    bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        std::string receivedData(buffer, bytesReceived);
        std::cout << "Primljen zahtev od klijenta: " << receivedData << std::endl;
        send(clientSocket, buffer, bytesReceived, 0);
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Greška pri inicijalizaciji Winsock-a. Kod greške: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Greška pri kreiranju soketa za slušanje." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP_ADDRESS, &serverAddr.sin_addr);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Greška pri povezivanju soketa." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Greška pri slušanju." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Load balancer radi na " << SERVER_IP_ADDRESS << ":" << SERVER_PORT << "..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Greška pri prihvatanju veze." << std::endl;
            continue;
        }

        std::thread(handleClient, clientSocket).detach();
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}