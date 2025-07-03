#include <iostream>
#include <thread>
#include <winsock2.h>
#include <vector>
#include <mutex>
#include "worker.h" 
#include "list.h"   
#include "queue.h"
#include "Distributor.h"
#include "message.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT_CLIENT 5059
#define SERVER_PORT_WORKER 6060
#define BUFFER_SIZE 256


std::mutex clientSocketsMutex;


int broj_obradjenih_poruka=0;
int counter = 0;

Node* workers = NULL;
std::mutex workersMutex;  

Queue* clientMessages = NULL;
std::mutex clientMessageQueueMutex;
std::vector<SOCKET> clientSockets;

int global_msg_id = 0;
static std::mutex idMutex;

std::mutex obradjeneLockMutex;


void printWorker(void* data) {
    Worker* worker = (Worker*)data;
    if (worker == NULL) {
        printf("Nevalidan Worker!\n");
        return;
    }

    printf("\nWorker ID: %d, Socket FD: %d, MessagesCount %d\n", worker->id, worker->socketFd, worker->dataCount);
}

int compareWorkersByPointer(void* a, void* b) {
    return (a == b) ? 0 : 1;
}



void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string leftover;

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                printf("LB: Klijent se diskonektovao (socket=%d)\n", (int)clientSocket);
            }
            else {
                printf("LB: Greška pri primanju od klijenta %d, code=%d\n",
                    (int)clientSocket, WSAGetLastError());
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        leftover += buffer;

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string oneMessage = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);  // ukloni poruku i delimiter

            // ukloni trailing \r ako postoji (Windows stil)
            if (!oneMessage.empty() && oneMessage.back() == '\r') {
                oneMessage.pop_back();
            }

            printf("LB: Primljena poruka od klijenta (socket=%d) => content=\"%s\"\n\n",
                (int)clientSocket, oneMessage.c_str());

            Message msg;

            // Kopiraj poruku u msg.content, pazi na veličinu
            strncpy_s(msg.content, oneMessage.c_str(), sizeof(msg.content) - 1);
            msg.content[sizeof(msg.content) - 1] = '\0';

            {
                std::lock_guard<std::mutex> lock(idMutex);
                msg.msg_id = global_msg_id++;
            }
            msg.clientSocket = clientSocket;
            msg.type = TEXT_MESSAGE;

            // Formiraj framed poruku sa delimiterima | i završnim \n
            char framed[BUFFER_SIZE] = { 0 };
            snprintf(framed, sizeof(framed), "|%d|%s|\n", msg.msg_id, msg.content);
            strncpy_s(msg.content, framed, sizeof(msg.content) - 1);
            msg.content[sizeof(msg.content) - 1] = '\0';

            {
                std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
                enqueue(clientMessages, &msg);
            }

            Worker* mostFreeWorker = nullptr;

            if (workers != nullptr) {
                {
                    std::lock_guard<std::mutex> lock(workersMutex);
                    mostFreeWorker = findMostFreeWorker(workers);
                }

                if (mostFreeWorker != nullptr) {
                    sendDataToWorker(mostFreeWorker, clientMessages);
                }
            }

            // Pošalji kratak ack klijentu
            char shortAck[BUFFER_SIZE];
            snprintf(shortAck, sizeof(shortAck), "LB primio msg_id=%d", msg.msg_id);
            send(clientSocket, shortAck, (int)strlen(shortAck), 0);
        }
    }

    closesocket(clientSocket);

    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        auto it = std::find(clientSockets.begin(), clientSockets.end(), clientSocket);
        if (it != clientSockets.end()) {
            clientSockets.erase(it);
        }
    }

    printf("Zavrsena nit za klijenta.\n");
}

void clientListener(SOCKET clientListenSocket) {
    while (true) {
        SOCKET clientSocket = accept(clientListenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            printf("Greska pri prihvatanju veze.\n");
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

void deleteWorkerNode(Node** head, Worker* worker) {
    Node* current = *head;
    Node* prev = nullptr;

    while (current != nullptr) {
        if ((Worker*)current->data == worker) {
            // Izbaci čvor iz liste
            if (prev == nullptr) {
                *head = current->next;
            }
            else {
                prev->next = current->next;
            }

            // Prvo oslobodi Worker resurse
            destroyWorker(worker);

            // Oslobodi Node strukturu
            free(current);

            return;
        }
        prev = current;
        current = current->next;
    }
}

void handleWorkerResponse(Worker* worker) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(worker->socketFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                printf("Worker %d se diskonektovao.\n", worker->id);
            }
            else {
                printf("Greska pri primanju odgovora od workera ID%d. Kod greske: %d\n", worker->id, WSAGetLastError());
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("Worker ID%d response: %s\n", worker->id, buffer);

        Message* finished = removeMessageFromWorker(worker);
        if (finished) {
           
            char ackBuffer[BUFFER_SIZE];
            snprintf(ackBuffer, sizeof(ackBuffer),
                "msg_id=%d obradjeno: %s",
                finished->msg_id, finished->content);

            send(finished->clientSocket, ackBuffer, (int)strlen(ackBuffer), 0);

            {
                std::lock_guard<std::mutex> lock(obradjeneLockMutex);
                broj_obradjenih_poruka++;
                //printf("LB: Ukupno obradjeno poruka = %d\n", broj_obradjenih_poruka);
            }

            free(finished);
        }
        else {
            printf("LB: (warning) no in flight message to pop for Worker ID=%d\n", worker->id);
            
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        while (true) {
            Message* msg = removeMessageFromWorker(worker);
            if (msg == NULL) break;
            printf("Poruka vracena u queue: %s\n", msg->content);          
            enqueue(clientMessages, msg);
            free(msg);
        }
    }


    {
        std::lock_guard<std::mutex> lock(workersMutex);
        deleteWorkerNode(&workers, worker);
    }

 

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        redistributeMessagesDead(clientMessages, workers);
    }

    printf("LB: handleWorkerResponse thread for Worker ID=%d ended.\n", worker->id);
}

#include <chrono>     

void workerListener(SOCKET workerListenSocket) {
    while (true) {
        SOCKET workerSocket = accept(workerListenSocket, NULL, NULL);
        if (workerSocket == INVALID_SOCKET) {
            printf("Greska pri prihvatanju veze od Workera.\n");
            continue;
        }

        printf("Worker povezan!\n");

        // Kreiranje novog workera
        Worker* newWorker = createWorker(static_cast<int>(getListSize(workers)));
        newWorker->socketFd = static_cast<int>(workerSocket);
        
        {
        std::lock_guard<std::mutex> lock(workersMutex); 

        insertAtEnd(&workers, newWorker, sizeof(Worker));
        printf("Novi Worker dodat sa ID: %d\n", newWorker->id);
 
        }

        if (getListSize(workers) > 1) {
            std::lock_guard<std::mutex> lock(workersMutex);
            redistributeMessages(clientMessages, workers);
        }
        displayList(workers, printWorker);

      
        char buffer[BUFFER_SIZE];
        int bytesReceived = recv(workerSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            //printf("Podaci od Workera: %s\n", buffer);
        }

   
        //printWorkerInfo(newWorker);
        //displayList(workers, printWorker);

        std::thread(handleWorkerResponse, newWorker).detach();

    }
}



int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greska pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    clientMessages = createQueue(BUFFER_SIZE);
    if (!clientMessages) {
        printf("Greska pri kreiranju reda klijenata.\n");
        WSACleanup();
        return 1;
    }

    SOCKET clientListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientListenSocket == INVALID_SOCKET) {
        printf("Greska pri kreiranju soketa.\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT_CLIENT);

    if (bind(clientListenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Greska pri bindenju soketa.\n");
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(clientListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Greska pri postavljanju slušanja na soketu.\n");
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Load Balancer osluskuje na portu: %d\n", SERVER_PORT_CLIENT);

    SOCKET workerListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (workerListenSocket == INVALID_SOCKET) {
        printf("Greska pri kreiranju Worker soketa.\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in workerAddress;
    workerAddress.sin_family = AF_INET;
    workerAddress.sin_addr.s_addr = INADDR_ANY;
    workerAddress.sin_port = htons(SERVER_PORT_WORKER);

    if (bind(workerListenSocket, (sockaddr*)&workerAddress, sizeof(workerAddress)) == SOCKET_ERROR) {
        printf("Greska pri bindenju Worker soketa.\n");
        closesocket(workerListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(workerListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Greska pri postavljanju slusanja na Worker soketu.\n");
        closesocket(workerListenSocket);
        WSACleanup();
        return 1;
    }

    std::thread(clientListener, clientListenSocket).detach();
    std::thread(workerListener, workerListenSocket).detach();


    while (true) {
     
        std::this_thread::sleep_for(std::chrono::seconds(1000));
    }

    closesocket(clientListenSocket);
    closesocket(workerListenSocket);
    WSACleanup();
    return 0;
}
