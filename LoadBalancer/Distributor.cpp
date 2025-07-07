#include "Distributor.h"
#include <iostream>
#include <fstream>

extern std::mutex clientMessageQueueMutex;
extern std::mutex workersMutex;

std::mutex logFileMutex;

void logMessage(const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Zaključaj mutex da niko ne prepiše u isto vreme
    {
        std::lock_guard<std::mutex> lock(logFileMutex);
        std::ofstream logFile("log.txt", std::ios::app);
        if (logFile.is_open()) {
            logFile << buffer << std::endl;
        }
    }

    // I ispiši u konzolu
    printf("%s\n", buffer);
}


Worker* findMostFreeWorker(Node* head)
{
    Node* current = head;
    Worker* worker = (Worker*)current->data;
    current = current->next;

    while (current != NULL)
    {
        Worker* nextWorker = (Worker*)current->data;
        {
            std::lock_guard<std::mutex> lock1(worker->mtx);
            std::lock_guard<std::mutex> lock2(nextWorker->mtx);
            if (worker->dataCount > nextWorker->dataCount)
            {
                worker = nextWorker;
            }
        }
        current = current->next;
    }

    return worker;
}

Worker* selectWorker(Node* workers) {
    Worker* kandidat = nullptr;

    Node* current = workers;
    while (current != NULL) {
        Worker* w = (Worker*)current->data;

        {
            std::lock_guard<std::mutex> lock(w->mtx);  // Zaključaj worker tokom provere
            if (w->isNew && w->dataCount < w->targetMsgCount) {
                if (kandidat == nullptr) {
                    kandidat = w;
                }
                else {
                    std::lock_guard<std::mutex> lock2(kandidat->mtx);
                    if (w->dataCount < kandidat->dataCount) {
                        kandidat = w;
                    }
                }
            }
        }

        current = current->next;
    }

    if (kandidat != nullptr) return kandidat;

    return findMostFreeWorker(workers);
}

bool isWorkerSocketAlive(Worker* worker) {
    if (!worker || worker->socketFd == -1)
        return false;

    char test;
    int result = recv(worker->socketFd, &test, 1, MSG_PEEK);
    if (result == 0) return false; // socket zatvoren
    if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) return false; // ozbiljna greška

    return true;
}

std::mutex sendDataMutex;
bool sendDataToWorker(Worker* worker, Queue* clientMessages) {
    if (worker == NULL) {
        printf("Nevalidan Worker!\n");
        return false;
    }

    /*
    if (!isWorkerSocketAlive(worker)) {
        printf("[sendDataToWorker] Worker ID=%d je zatvoren. Prekidam slanje.\n", worker->id);
        return false;
    }
    */

    std::lock_guard<std::mutex> lock(sendDataMutex);

    Message localMsg;
    bool validMsg = false;
    {
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        if (!isEmpty(clientMessages)) {
            dequeue(clientMessages, &localMsg);
            validMsg = true;
        }
    }

    if (!validMsg) {
        printf("Pokušaj slanja, ali red je prazan! Preskačem.\n");
        return false;
    }

    // Dinamička kopija koja se koristi kroz celu funkciju
    Message* msgCopy = (Message*)malloc(sizeof(Message));
    if (!msgCopy) {
        fprintf(stderr, "Greška pri alokaciji memorije za msgCopy.\n");
        // Vraćanje poruke nazad u queue
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        enqueue(clientMessages, &localMsg);
        return false;
    }
    memcpy(msgCopy, &localMsg, sizeof(Message));

    bool added = addMessageToWorker(worker, msgCopy);
    if (!added) {
        printf("Worker ID=%d je pun, poruka msg_id=%d nije dodata. Vracam poruku u red.\n", worker->id, msgCopy->msg_id);
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        enqueue(clientMessages, msgCopy);
        free(msgCopy);
        return false;
    }

    int len = (int)strlen(msgCopy->content);
    int sent = send(worker->socketFd, msgCopy->content, len, 0);

    if (sent == SOCKET_ERROR) {
        printf("Greška pri slanju workeru (ID=%d).\n", worker->id);
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        enqueue(clientMessages, msgCopy);
        free(msgCopy);
        return false;
    }
    else {
        printf("Poslato %d bajta workeru (ID=%d): %s\n", sent, worker->id, msgCopy->content);
        free(msgCopy);
        return true;
    }
}


//ako worker opadne a svi workeri su vec primili poruke i samo obradjuju

void redistributeMessagesDead(Queue* clientMessages, Node* workers) {

    while (true) {
        {

            std::lock_guard<std::mutex> queueLock(clientMessageQueueMutex);
            if (isEmpty(clientMessages)) {
                break;
            }
        }

        Worker* bestWorker = nullptr;
        {

            bestWorker = findMostFreeWorker(workers);
        }

        if (bestWorker == nullptr) {
            std::cerr << "Nema dostupnih workera za redistribuciju.\n";
            break;
        }

        bool sent = sendDataToWorker(bestWorker, clientMessages);
        if (!sent) {
            // Worker je možda pun ili došlo je do greške, napravi pauzu
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }



}
