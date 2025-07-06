#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

#define REPLICATOR_PORT 6061
#define BUFFER_SIZE 256

std::mutex fileMutex;

void saveData(const char* buffer) {
    std::lock_guard<std::mutex> lock(fileMutex); 

    std::ofstream file("replicatorOutput.txt", std::ios::app);
    if (!file) {
        std::cerr << "Greska pri otvaranju fajla." << std::endl;
        return;
    }
    file << buffer << std::endl;
}

void receiveAndStoreFromWorker(SOCKET workerSocket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(workerSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) break;

        buffer[bytesReceived] = '\0';
        std::string prefixed = "Replikator primio poruku: " + std::string(buffer);

        printf("[Replikator] Poruka uspesno replikovana: %s\n", prefixed.c_str());
        saveData(buffer);
    }

    closesocket(workerSocket);
}

int main() {
    std::ofstream("replicatorOutput.txt", std::ios::trunc).close();
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greska pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        printf("Greska pri kreiranju socket-a.\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(REPLICATOR_PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Greska pri bindovanju.\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Greska pri slušanju.\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    printf("Replikator ceka konekcije na portu %d...\n", REPLICATOR_PORT);

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET workerSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (workerSocket == INVALID_SOCKET) break;

        std::thread(receiveAndStoreFromWorker, workerSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}