#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "../Common/queue.h"

#pragma comment(lib, "ws2_32.lib")

#define REPLICATOR_PORT 6061
#define BUFFER_SIZE 256

Queue* replicationQueue = nullptr;
std::mutex queueMutex;
std::condition_variable queueCV;
std::atomic<bool> stopReplikator(false);

void printMessageData(void* data) {
    printf("%s ", (char*)data);
}

void receiveAndStoreFromWorker(SOCKET workerSocket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(workerSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) break;

        buffer[bytesReceived] = '\0';

      
        std::string prefixed = "R|" + std::string(buffer);

        {
            std::lock_guard<std::mutex> lock(queueMutex);

            char* prefixedCopy = (char*)malloc(BUFFER_SIZE);
            if (!prefixedCopy) {
                printf("Greska pri alokaciji memorije za prefiks poruke.\n");
                continue;
            }

            strncpy_s(prefixedCopy, BUFFER_SIZE, prefixed.c_str(), _TRUNCATE);
            prefixedCopy[BUFFER_SIZE - 1] = '\0';

            enqueue(replicationQueue, prefixedCopy);
            free(prefixedCopy);
        }

        queueCV.notify_one();

        printf("[Replikator] Poruka uspesno replikovana: %s\n", prefixed.c_str());
        printf("[Replikator] Trenutni sadrzaj replicationQueue: ");
        displayList(replicationQueue->front, printMessageData);
        printf("\n");
    }

    closesocket(workerSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greska pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    replicationQueue = createQueue(BUFFER_SIZE);
    if (!replicationQueue) {
        printf("Greska pri kreiranju replication queue.\n");
        WSACleanup();
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

        std::thread t(receiveAndStoreFromWorker, workerSocket);
        t.detach();
    }

    stopReplikator.store(true);
    queueCV.notify_all();

    closesocket(serverSocket);
    freeQueue(replicationQueue);
    WSACleanup();
    return 0;
}
