#include "Distributor.h"
#include "message.h"
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

std::mutex sendDataMutex;

bool sendDataToWorker(Worker* worker, Queue* clientMessages) {
    if (worker == NULL) {
        printf("Nevalidan Worker!\n");
        return false;
    }

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

std::atomic<bool> isFREE_QUEUE_ACTIVE(false);

void sendFreeQueueCommandToWorker(Worker* worker) {

    const char* freeQueueCommand = "FREE_QUEUE\n";
    int sent = send(worker->socketFd, freeQueueCommand, strlen(freeQueueCommand), 0);
    if (sent == SOCKET_ERROR) {
        printf("Greska pri slanju FREE_QUEUE workeru (ID=%d).\n", worker->id);
    }
}

#include <atomic>
std::atomic<bool> isRedistributeActive(false);

//ako worker opadne a svi workeri su vec primili poruke i samo obradjuju

void redistributeMessagesDead(Queue* clientMessages, Node* workers) {

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (isRedistributeActive.load()) {
        printf("Glavna redistribucija je aktivna. Dead redistribucija se prekida.\n");
        return;
    }

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

void redistributeMessages(Queue* clientMessages, Node* workers) {

    isRedistributeActive.store(true);

    //Slanje FREE_QUEUE komande svim workerima
    {

        Node* current = workers;
        while (current != NULL && current->next != NULL) {
            Worker* worker = (Worker*)current->data;
            sendFreeQueueCommandToWorker(worker);
            current = current->next;
        }
    }

    //Prikupi sve neobrađene poruke sa svih workera i prebaci ih u clientMessages
    {
       
        Node* current = workers;
        while (current != NULL) {
            Worker* worker = (Worker*)current->data;
            {
             
                while (worker->dataCount > 0) {
                    Message* msgPtr = removeMessageFromWorker(worker);
                    Message safeCopy;
                    memcpy(&safeCopy, msgPtr, sizeof(Message));
                    //printf("[REDIST] msg_id=%d content=\"%s\"\n", safeCopy.msg_id, safeCopy.content);
                    logMessage("[REDIST] msg_id=%d content=\"%s\"", safeCopy.msg_id, safeCopy.content);
                    if (msgPtr != NULL) {
                        {
                            std::lock_guard<std::mutex> queueLock(clientMessageQueueMutex);
                            enqueue(clientMessages, msgPtr);
                            printf("DBG: content = \"%s\"\n", msgPtr->content);
                        }
                        free(msgPtr); 
                    }
                }
            }
            current = current->next;
        }
    }

    //Uzimanje poruka iz clientMessages i slanje najraspolozivijem worker-u
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

        sendDataToWorker(bestWorker, clientMessages);
    }
    isRedistributeActive.store(false);

}