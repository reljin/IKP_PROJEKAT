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

    newWorker->inflightMessages = createMessageMap(1024);

    return newWorker;
}

bool addMessageToWorker(Worker* worker, const Message* newMessage) {
    std::lock_guard<std::mutex> lock(worker->mtx);

    if (worker->dataCount >= MAX_DATA_SIZE) {
        printf("Nema dovoljno prostora za novu poruku.\n");
        return false;
    }

    Message* msgCopy = (Message*)malloc(sizeof(Message));
    if (!msgCopy) {
        printf("Greska pri alokaciji memorije za novu poruku.\n");
        return false;
    }
    memcpy(msgCopy, newMessage, sizeof(Message));

    worker->data[worker->dataCount++] = msgCopy;
    insertMessage(worker->inflightMessages, msgCopy->msg_id, msgCopy);

    return true;
}

Message* removeMessageFromWorkerByMessageId(Worker* worker, int msg_id) {
    std::lock_guard<std::mutex> lock(worker->mtx);

    Message* msg = getMessage(worker->inflightMessages, msg_id);
    if (!msg) return nullptr;

    int found = -1;
    for (int i = 0; i < worker->dataCount; ++i) {
        if (worker->data[i]->msg_id == msg_id) {
            found = i;
            break;
        }
    }

    if (found != -1) {
        for (int j = found; j < worker->dataCount - 1; ++j) {
            worker->data[j] = worker->data[j + 1];
        }
        worker->data[--worker->dataCount] = nullptr;
    }

    removeMessage(worker->inflightMessages, msg_id);
    return msg;
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
    removeMessage(worker->inflightMessages, removedMessage->msg_id);

    return removedMessage;
}

void destroyWorker(Worker* worker) {
    if (!worker) return;

    for (int i = 0; i < worker->dataCount; ++i) {
        if (worker->data[i]) free(worker->data[i]);
    }
    free(worker->data);

    freeMessageMap(worker->inflightMessages);
    delete worker;
}

void printWorkerInfo(const Worker* worker) {
    printf("ID workera: %d\n", worker->id);
    printf("Broj poruka: %d\n", worker->dataCount);
    printf("Maksimalan broj poruka: %d\n", MAX_DATA_SIZE);
    printf("Socket FD: %d\n", worker->socketFd);
    printf("Adresa workera: %u\n", worker->addr.sin_addr.s_addr);
}
