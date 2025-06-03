#include <iostream>
#include <thread>
#include <winsock2.h>
#include <vector>
#include <mutex>
#include "worker.h"  // Uključujemo header sa Worker strukturom
#include "list.h"    // Ukoliko koristiš listu za skladištenje workera
#include "queue.h"
#include "Distributor.h"
#include "message.h"
#include "hashset.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT_CLIENT 5059
#define SERVER_PORT_WORKER 6060
#define BUFFER_SIZE 256


std::mutex clientSocketsMutex;
std::vector<SOCKET> clientSockets;


int broj_obradjenih_poruka=0;
int counter = 0;

Node* workers = NULL;
std::mutex workersMutex;  // Mutex za zaštitu liste workera

Queue* clientMessages = NULL;
std::mutex clientMessageQueueMutex;

int global_msg_id = 0;
static std::mutex idMutex;

std::mutex obradjeneLockMutex;

Node* sentMessages = NULL;  // globalna lista svih poslatih poruka

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

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                printf("LB: Klijent se diskonektovao (socket=%d)\n", (int)clientSocket);
            }
            else {
                printf("LB: Greska pri primanju od klijenta %d, code=%d\n",
                    (int)clientSocket, WSAGetLastError());
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("LB: Primljena poruka od klijenta (socket=%d) => content=\"%s\"\n",
        (int)clientSocket, buffer);

        Message msg;      

        msg.content[BUFFER_SIZE - 1] = '\0';  // just in case
        strncpy_s(msg.content, buffer, sizeof(msg.content) - 1);

        // If client sent msg_id=0, assign a new one (otherwise respect any ID the client included)

        {
            std::lock_guard<std::mutex> lock(idMutex);
            msg.msg_id = global_msg_id++;

        }
        msg.clientSocket = clientSocket;
        msg.type = TEXT_MESSAGE;

        {
            std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
            enqueue(clientMessages, &msg);
        }
        

        Worker* mostFreeWorker = nullptr;

        

        if (workers != NULL) {
            {
                
                std::lock_guard<std::mutex> lock(workersMutex);
                mostFreeWorker = findMostFreeWorker(workers); 
            }

            if (mostFreeWorker != nullptr) {
                sendDataToWorker(mostFreeWorker, clientMessages);

                Message* copy = (Message*)malloc(sizeof(Message));
                memcpy(copy, &msg, sizeof(Message));  // napravi kopiju poslate poruke
                insertAtEnd(&sentMessages, copy, sizeof(Message));
            }
        }
        //ACK back to client that we received it
        char shortAck[BUFFER_SIZE];
        snprintf(shortAck, sizeof(shortAck),
            "LB primio msg_id=%d", msg.msg_id);
        send(clientSocket, shortAck, (int)strlen(shortAck), 0);
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
                break;
            }
            break;  // Prekidamo petlju ako dođe do greške ili diskonektovanja
        }
        buffer[bytesReceived] = '\0';
        printf("Worker ID%d response: %s\n", worker->id, buffer);

        Message* finished = removeMessageFromWorker(worker);
        if (finished) {

            //Provera dal je id poruke dodat u hashset, ako jeste onda je poruka obradjena i ipak ne treba opet da se salje

            if (hashset_contains(finished->msg_id)) {
                printf("LB: msg_id=%d je već obrađena. Preskačem.\n", finished->msg_id);
                free(finished);
                continue;
            }

            // ACK originalnom klijentu
            char ackBuffer[BUFFER_SIZE];
            snprintf(ackBuffer, sizeof(ackBuffer),
                "msg_id=%d obradjeno: %s",
                finished->msg_id, finished->content);

            send(finished->clientSocket, ackBuffer, (int)strlen(ackBuffer), 0);

            // Zabeleži kao obrađenu
            hashset_add(finished->msg_id);

            {
                std::lock_guard<std::mutex> lock(obradjeneLockMutex);
                broj_obradjenih_poruka++;
                printf("LB: Ukupno obradjeno poruka = %d\n", broj_obradjenih_poruka);
            }

            free(finished);
        }
        else {
            printf("LB: (warning) no in flight message to pop for Worker ID=%d\n", worker->id);
        }

    }


    //AKO SE WORKER DISKONEKTUJE , skini mu sve poruke, uradi redistribuciju 
    {
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);  // Zaključavanje queue-a

        while (true) {
            Message* msg = removeMessageFromWorker(worker);  
            if (msg == NULL) break;  
            printf("Poruka vracena u queue: %s\n", msg->content);
            enqueue(clientMessages, msg);
            free(msg);
            
        }
    }

    {
        std::lock_guard<std::mutex> lock(workersMutex);  // Zaključavanje mutex-a
        deleteNode(&workers, worker, sizeof(Worker), compareWorkersByPointer);
        
    }
    destroyWorker(worker);

    {
        std::lock_guard<std::mutex> lock(workersMutex);
        redistributeMessages(clientMessages, workers);
    }
    //displayList(workers, printWorker);

    printf("LB: handleWorkerResponse thread for Worker ID=%d ended.\n", worker->id);

}

#include <chrono>       // C++ vreme
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
        
        {
        std::lock_guard<std::mutex> lock(workersMutex);  // Zaključavanje mutex-a

        insertAtEnd(&workers, newWorker, sizeof(Worker));
        printf("Novi Worker dodat sa ID: %d\n", newWorker->id);
 
        }

        if (getListSize(workers) > 1) {
            std::lock_guard<std::mutex> lock(workersMutex);
            redistributeMessages(clientMessages, workers);
        }
        displayList(workers, printWorker);

        // Ovdje možemo da komuniciramo sa workerom
        char buffer[BUFFER_SIZE];
        int bytesReceived = recv(workerSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            //printf("Podaci od Workera: %s\n", buffer);
        }

        // Ispisivanje informacija o novom workeru
        //printWorkerInfo(newWorker);
        //displayList(workers, printWorker);

        std::thread(handleWorkerResponse, newWorker).detach();

    }
}

void sendMessageToWorker(Worker* worker, Message* msg) {
    send(worker->socketFd, msg->content, (int)strlen(msg->content), 0);
    addMessageToWorker(worker, msg);  // Ako koristiš ovu funkciju za tracking
}

void retryUnacknowledgedMessages() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));  // Proverava svakih 5 sekundi

        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);  // Pristup sentMessages i redovima
        Node* current = sentMessages;
        while (current != NULL) {
            Message* msg = (Message*)current->data;
            if (!hashset_contains(msg->msg_id)) {
                printf("RETRY: msg_id=%d nije potvrđen, ponovo šaljem poruku: \"%s\"\n", msg->msg_id, msg->content);

                Worker* targetWorker = nullptr;
                {
                    std::lock_guard<std::mutex> lock(workersMutex);
                    targetWorker = findMostFreeWorker(workers);
                }

                if (targetWorker != nullptr) {
                    // Možeš direktno slati poruku ovom workeru
                    sendMessageToWorker(targetWorker, msg);
                }
            }
            current = current->next;
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greška pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    clientMessages = createQueue(BUFFER_SIZE);
    if (!clientMessages) {
        printf("Greška pri kreiranju reda klijenata.\n");
        WSACleanup();
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
    std::thread(retryUnacknowledgedMessages).detach();

    while (true) {
        //printf("Load Balancer radi druge poslove...\n");
        std::this_thread::sleep_for(std::chrono::seconds(1000));
    }

    closesocket(clientListenSocket);
    closesocket(workerListenSocket);
    WSACleanup();
    return 0;
}
