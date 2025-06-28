#include "worker.h"
#include <iostream>
#include <cstring> 

Worker* createWorker(int id) {

    Worker* newWorker = new Worker;
    if (newWorker == nullptr) {
        printf("Greska pri alokaciji memorije za workera.\n");
        return nullptr;
    }

    newWorker->id = id;
    newWorker->dataCount = 0;

    newWorker->data = new Message * [MAX_DATA_SIZE];
    if (newWorker->data == nullptr) {
        printf("Greska pri alokaciji memorije za niz poruka.\n");
        delete newWorker;
        return nullptr;
    }

    for (int i = 0; i < MAX_DATA_SIZE; i++) {
        newWorker->data[i] = nullptr;
    }

    newWorker->socketFd = -1;

    return newWorker;
}

//zakljucavanje je vec ubaceno!!!
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
            delete worker->data[i];
        }
    }

    delete[] worker->data;
    delete worker;
}

void printWorkerInfo(const Worker* worker) {
    printf("ID workera: %d\n", worker->id);
    printf("Broj poruka: %d\n", worker->dataCount);
    printf("Maksimalan broj poruka: %d\n", MAX_DATA_SIZE);
    printf("Socket FD: %d\n", worker->socketFd);
    printf("Adresa workera: %u\n", worker->addr.sin_addr.s_addr);
}
