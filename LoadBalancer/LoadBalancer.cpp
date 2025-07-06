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

void printQueue(Queue* q) {
    if (q == nullptr) {
        printf("Queue je NULL.\n");
        return;
    }

    Node* current = q->front;
    int i = 0;
    while (current != nullptr) {
        Message* msg = (Message*)current->data;
        if (msg != nullptr) {
            printf("  [%d] msg_id=%d | content=\"%s\" | socket=%d\n", i++, msg->msg_id, msg->content, msg->clientSocket);
        }
        else {
            printf("  [%d] msg=NULL\n", i++);
        }
        current = current->next;
    }

    if (i == 0) {
        printf("  Queue je prazan.\n");
    }
}

void disconnectClient(SOCKET clientSocket) {
    printf("Prekidam vezu sa klijentom (socket=%d)...\n", (int)clientSocket);

    closesocket(clientSocket);

    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        auto it = std::find(clientSockets.begin(), clientSockets.end(), clientSocket);
        if (it != clientSockets.end()) {
            clientSockets.erase(it);
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        removeMessagesFromQueueBySocket(clientMessages, clientSocket);
    }

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        removeMessagesFromAllWorkersBySocket(workers, clientSocket);
    }

    printf("Klijent (socket=%d) je uklonjen iz svih struktura.\n", (int)clientSocket);
}

#include <random>
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(200, 1000);

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string leftover;
    int waitTimeProgress = 0;

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
            disconnectClient(clientSocket);  
            break;
        }
        buffer[bytesReceived] = '\0';
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        //printf("[LB DEBUG] Primljeno %d bajtova od socket=%d: \"%.*s\"\n",bytesReceived, (int)clientSocket, bytesReceived, buffer);       
        leftover += buffer;

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string oneMessage = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);  
         
            if (!oneMessage.empty() && oneMessage.back() == '\r') {
                oneMessage.pop_back();
            }

            printf("LB: Primljena poruka od klijenta (socket=%d) => content=\"%s\"\n\n",
                (int)clientSocket, oneMessage.c_str());

            Message msg;           
            strncpy_s(msg.content, oneMessage.c_str(), sizeof(msg.content) - 1);
            msg.content[sizeof(msg.content) - 1] = '\0';
            {
                std::lock_guard<std::mutex> lock(idMutex);
                msg.msg_id = global_msg_id++;
            }
            msg.clientSocket = clientSocket;
            msg.type = TEXT_MESSAGE;

            
            char framed[BUFFER_SIZE] = { 0 };
            snprintf(framed, sizeof(framed), "|%d|%s|\n", msg.msg_id, msg.content);
            strncpy_s(msg.content, framed, sizeof(msg.content) - 1);
            msg.content[sizeof(msg.content) - 1] = '\0';

            {
                std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
                enqueue(clientMessages, &msg);
            }

            Worker* mostFreeWorker = nullptr;
            int retryCount = 0;
            const int MAX_RETRIES = 5;

            bool success = false;

            while (!success) {
                {
                    std::lock_guard<std::mutex> lock(workersMutex);
                    mostFreeWorker = selectWorker(workers);
                }

                if (mostFreeWorker != nullptr) {

                    success = sendDataToWorker(mostFreeWorker, clientMessages);
                    if (!success) {
                        printf("Slanje poruke NIJE uspelo (pokusaj %d). Pauza...\n", retryCount + 1);

                        int sleep_ms = dis(gen);
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms + waitTimeProgress));
                        waitTimeProgress += 1000;

                        if (waitTimeProgress > 5000)
                            waitTimeProgress = 5000;

                        retryCount++;

                        if (retryCount == MAX_RETRIES) {
                            printf("Workeri su pretrpani");
                            disconnectClient(clientSocket);

                            break;
                        }

                    }
                    else {
                        waitTimeProgress -= 200;
                        if (waitTimeProgress < 0)
                            waitTimeProgress = 0;
                    }
                }
                else {
                    printf("Nema dostupnih Workera! Cekam...\n");
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    retryCount++;
                }
            }
            // Pošalji kratak ack klijentu
            //char shortAck[BUFFER_SIZE];
            //snprintf(shortAck, sizeof(shortAck), "LB primio msg_id=%d", msg.msg_id);
            //send(clientSocket, shortAck, (int)strlen(shortAck), 0);
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        printf("\n[LB] Ispis clientMessages queue:\n");
        printQueue(clientMessages);
    }

    if (!leftover.empty()) {
        printf("Preostali nepotpuni podaci od klijenta (socket=%d): \"%s\"\n", (int)clientSocket, leftover.c_str());
        leftover.clear(); // ili std::string().swap(leftover);
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
          
            if (prev == nullptr) {
                *head = current->next;
            }
            else {
                prev->next = current->next;
            }

            
            destroyWorker(worker);

            free(current);

            return;
        }
        prev = current;
        current = current->next;
    }
}

#include <fstream>
std::mutex workerResponseLogMutex;

void logWorkerResponse(int workerId, const char* buffer, int bytesReceived) {
    std::lock_guard<std::mutex> lock(workerResponseLogMutex);

    std::ofstream file("workerResponses.txt", std::ios::app);
    if (!file) return;

    file << "[Worker " << workerId << "] ";
    file.write(buffer, bytesReceived);
    file << "\n";
    file.close();
}

void handleWorkerResponse(Worker* worker) {
    char buffer[BUFFER_SIZE];
    std::string leftover;  // Bafer za slabo primljene ili spojene poruke

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
        leftover.append(buffer, bytesReceived);

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string oneMessage = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);
            printf("Worker ID%d response: %s\n", worker->id, oneMessage.c_str());
            logWorkerResponse(worker->id, oneMessage.c_str(), (int)oneMessage.size());


            int msg_id = -1;
            msg_id = extractMsgIdFromWorkerResponse(oneMessage);
            if (msg_id == -1) {
                printf("LB: [GRESKA] Nevalidna poruka od Workera %d: %s\n", worker->id, oneMessage.c_str());
                continue;
            }
            Message* finished = removeMessageFromWorkerByMessageId(worker, msg_id);
            if (finished) {
                char ackBuffer[BUFFER_SIZE];

                snprintf(ackBuffer, sizeof(ackBuffer),"msg_id=%d obradjeno: %s",finished->msg_id, finished->content);
                send(finished->clientSocket, ackBuffer, (int)strlen(ackBuffer), 0);
                
                {
                    std::lock_guard<std::mutex> lock(obradjeneLockMutex);
                    broj_obradjenih_poruka++;
                    printf("LB: Ukupno obradjeno poruka = %d\n", broj_obradjenih_poruka);
                }

                free(finished);
            }
            else {
                printf("LB: (warning) no in flight message to pop for Worker ID=%d\n", worker->id);
                printf("LB: [WARNING] Nema poruke za msg_id=%d kod Workera ID=%d\n", msg_id, worker->id);
                //proveri dal je dobar removeMessageFromWorker(worker);
            }
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

    leftover.clear();
    std::string().swap(leftover);

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

            int ukupno = 0;
            int brojWR = 0;

            Node* current = workers;
            while (current != NULL) {
                Worker* w = (Worker*)current->data;
                if (w != newWorker && !w->isNew) {
                    ukupno += w->dataCount;
                    brojWR++;
                }
                current = current->next;
            }

            int prosek = brojWR > 0 ? ukupno / brojWR : 0;
            float faktor = 0.6f;  // koliko % proseka novi WR treba da primi
            std::lock_guard<std::mutex> newWorkerLock(newWorker->mtx);
            newWorker->targetMsgCount = (int)(prosek * faktor);
            newWorker->isNew = true;

            printf("[BALANCE] Novi Worker ID=%d dobija %d poruka pre nego što uđe u ravnotežu.\n",newWorker->id, newWorker->targetMsgCount);
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
    std::ofstream("workerResponses.txt", std::ios::trunc).close();
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greska pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    clientMessages = createQueue(sizeof(Message));
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
