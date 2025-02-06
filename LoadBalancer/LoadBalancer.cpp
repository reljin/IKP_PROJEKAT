#include <iostream>
#include <thread>
#include <winsock2.h>
#include <vector>
#include <mutex>
#include "worker.h"  // Uključujemo header sa Worker strukturom
#include "list.h"    // Ukoliko koristiš listu za skladištenje workera

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT_CLIENT 5059
#define SERVER_PORT_WORKER 6060
#define BUFFER_SIZE 256

std::mutex clientSocketsMutex;
std::vector<SOCKET> clientSockets;
Node* workers = NULL;

void printWorker(void* data) {
    Worker* worker = (Worker*)data;
    if (worker == NULL) {
        printf("Nevalidan Worker!\n");
        return;
    }

    printf("\nWorker ID: %d, Socket FD: %d\n", worker->id, worker->socketFd);
}

// Funkcija za slanje poruke workeru
void sendDataToWorker(Worker* worker, const char* message) {
    if (worker == NULL) {
        printf("Nevalidan Worker!\n");
        return;
    }

    // Dodajemo poruku u worker
    Message msg = { TEXT_MESSAGE, "" };
    strncpy_s(msg.content, message, sizeof(msg.content) - 1);
    msg.content[sizeof(msg.content) - 1] = '\0';

    addMessageToWorker(worker, &msg);
    printf("Ne znam zasto ne radim");
    // Ako želimo da pošaljemo poruku putem soketa
    int bytesSent = send(worker->socketFd, message, static_cast<int>(strlen(message)), 0);  // Ispravljeno
    if (bytesSent == SOCKET_ERROR) {
        printf("Greška pri slanju poruke workeru.\n");
    }
    else {
        printf("Poruka poslata workeru sa ID: %d\n", worker->id);
    }
}

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                printf("Klijent se diskonektovao.\n");
            }
            else {
                printf("Greška pri primanju podataka. Kod greške: %d\n", WSAGetLastError());
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("Poruka od klijenta: %s\n", buffer);

        // Pretpostavljamo da želimo da šaljemo poruku prvom dostupnom workeru
        if (workers != NULL) {
            Worker* firstWorker = (Worker*)workers->data;
            sendDataToWorker(firstWorker, buffer);
        }

        std::string response = "Poruka primljena: " + std::string(buffer);
        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    }

    closesocket(clientSocket);

    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        auto it = std::find(clientSockets.begin(), clientSockets.end(), clientSocket);
        if (it != clientSockets.end()) {
            clientSockets.erase(it);
        }
    }

    printf("Završena nit za klijenta.\n");
}

void clientListener(SOCKET clientListenSocket) {
    while (true) {
        SOCKET clientSocket = accept(clientListenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            printf("Greška pri prihvatanju veze.\n");
            continue;
        }

        printf("Klijent povezan!\n");

        {
            std::lock_guard<std::mutex> lock(clientSocketsMutex);
            clientSockets.push_back(clientSocket);
        }

        std::thread(handleClient, clientSocket).detach();
    }
}


// Funkcija za osvežavanje liste workera u Load Balanceru
void workerListener(SOCKET workerListenSocket) {
    while (true) {
        SOCKET workerSocket = accept(workerListenSocket, NULL, NULL);
        if (workerSocket == INVALID_SOCKET) {
            printf("Greška pri prihvatanju veze od Workera.\n");
            continue;
        }

        printf("Worker povezan!\n");

        // Kreiranje novog workera
        Worker* newWorker = createWorker(static_cast<int>(getListSize(workers)));
        newWorker->socketFd = static_cast<int>(workerSocket);

        // Dodajemo novog workera u listu workera
        insertAtEnd(&workers, newWorker, sizeof(Worker));

        // Ovdje možemo da komuniciramo sa workerom
        char buffer[BUFFER_SIZE];
        int bytesReceived = recv(workerSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            printf("Podaci od Workera: %s\n", buffer);
        }

        // Ispisivanje informacija o novom workeru
        printWorkerInfo(newWorker);
        displayList(workers, printWorker);

        // Ne zatvaramo soket ovde, jer ćemo ga koristiti za dalju komunikaciju
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greška pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    SOCKET clientListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientListenSocket == INVALID_SOCKET) {
        printf("Greška pri kreiranju soketa.\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT_CLIENT);

    if (bind(clientListenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Greška pri bindenju soketa.\n");
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(clientListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Greška pri postavljanju slušanja na soketu.\n");
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Load Balancer osluškuje na portu: %d\n", SERVER_PORT_CLIENT);

    SOCKET workerListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (workerListenSocket == INVALID_SOCKET) {
        printf("Greška pri kreiranju Worker soketa.\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in workerAddress;
    workerAddress.sin_family = AF_INET;
    workerAddress.sin_addr.s_addr = INADDR_ANY;
    workerAddress.sin_port = htons(SERVER_PORT_WORKER);

    if (bind(workerListenSocket, (sockaddr*)&workerAddress, sizeof(workerAddress)) == SOCKET_ERROR) {
        printf("Greška pri bindenju Worker soketa.\n");
        closesocket(workerListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(workerListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Greška pri postavljanju slušanja na Worker soketu.\n");
        closesocket(workerListenSocket);
        WSACleanup();
        return 1;
    }

    std::thread(clientListener, clientListenSocket).detach();
    std::thread(workerListener, workerListenSocket).detach();

    while (true) {
        printf("Load Balancer radi druge poslove...\n");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    closesocket(clientListenSocket);
    closesocket(workerListenSocket);
    WSACleanup();
    return 0;
}
