#include "worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Worker* createWorker(int id) {
    Worker* newWorker = new Worker(); // koristi new, ne malloc
    if (newWorker == nullptr) {
        printf("Greska pri alokaciji memorije za workera.\n");
        return nullptr;
    }

    newWorker->id = id;
    newWorker->dataCount = 0;
    newWorker->socketFd = -1;
    newWorker->isNew = false;
    newWorker->targetMsgCount = 0;

    newWorker->data = (Message**)malloc(sizeof(Message*) * MAX_DATA_SIZE);
    if (newWorker->data == nullptr) {
        printf("Greska pri alokaciji memorije za niz poruka.\n");
        delete newWorker;  // koristi delete
        return nullptr;
    }

    for (int i = 0; i < MAX_DATA_SIZE; i++) {
        newWorker->data[i] = nullptr;
    }

    return newWorker;
}

bool addMessageToWorker(Worker* worker, const Message* newMessage) {
    std::lock_guard<std::mutex> lock(worker->mtx);

    if (worker->dataCount >= MAX_DATA_SIZE) {
        printf("Nema dovoljno prostora za novu poruku.\n");
        return false;
    }

    worker->data[worker->dataCount] = (Message*)malloc(sizeof(Message));
    if (worker->data[worker->dataCount] == NULL) {
        printf("Greska pri alokaciji memorije za novu poruku.\n");
        return false;
    }

    memcpy(worker->data[worker->dataCount], newMessage, sizeof(Message));
    worker->dataCount++;

    return true;
}

Message* removeMessageFromWorker(Worker* worker) {
    std::lock_guard<std::mutex> lock(worker->mtx);

    if (worker->dataCount == 0) {
        return NULL;
    }

    Message* removedMessage = worker->data[0];

    for (int i = 1; i < worker->dataCount; i++) {
        worker->data[i - 1] = worker->data[i];
    }

    worker->data[worker->dataCount - 1] = NULL;
    worker->dataCount--;

    return removedMessage;
}

void destroyWorker(Worker* worker) {
    if (worker == nullptr) return;

    for (int i = 0; i < worker->dataCount; i++) {
        if (worker->data[i] != nullptr) {
            free(worker->data[i]);
        }
    }

    free(worker->data);
    delete worker; // koristi delete, jer si koristio new
}

void printWorkerInfo(const Worker* worker) {
    printf("ID workera: %d\n", worker->id);
    printf("Broj poruka: %d\n", worker->dataCount);
    printf("Maksimalan broj poruka: %d\n", MAX_DATA_SIZE);
    printf("Socket FD: %d\n", worker->socketFd);
    printf("Adresa workera: %u\n", worker->addr.sin_addr.s_addr);
}
