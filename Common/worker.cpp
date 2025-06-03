#include "worker.h"
#include <iostream>
#include <cstring> // Za memcpy

Worker* createWorker(int id) {
    // Alociramo Workera
    Worker* newWorker = new Worker;
    if (newWorker == nullptr) {
        std::cerr << "Greška pri alokaciji memorije za Workera." << std::endl;
        return nullptr;
    }

    newWorker->id = id;
    newWorker->dataCount = 0;

    // Alociramo niz pokazivača na Message (koristimo new[])
    newWorker->data = new Message * [MAX_DATA_SIZE];
    if (newWorker->data == nullptr) {
        std::cerr << "Greška pri alokaciji memorije za niz poruka." << std::endl;
        delete newWorker;
        return nullptr;
    }

    // Postavljamo sve elemente na nullptr
    for (int i = 0; i < MAX_DATA_SIZE; i++) {
        newWorker->data[i] = nullptr;
    }

    newWorker->socketFd = -1;  // Inicijalno socket nije postavljen

    // Ostali članovi klase/strukture (mutex, sockaddr_in) će biti default-inicijalizovani

    return newWorker;
}

// Funkcija za dodavanje nove poruke (FIFO podržano)
bool addMessageToWorker(Worker* worker, const Message* newMessage) {
    std::lock_guard<std::mutex> lock(worker->mtx);

    // Proveravamo da li je worker pun
    if (worker->dataCount >= MAX_DATA_SIZE) {
        std::cerr << "Nema dovoljno prostora za novu poruku." << std::endl;
        return false;
    }

    // Alociramo novu poruku
    worker->data[worker->dataCount] = (Message*)malloc(sizeof(Message));
    if (worker->data[worker->dataCount] == NULL) {
        std::cerr << "Greška pri alokaciji memorije za novu poruku." << std::endl;
        return false;
    }

    // Kopiramo sadržaj poruke
    memcpy(worker->data[worker->dataCount], newMessage, sizeof(Message));
    worker->dataCount++;

    return true;
}

// Funkcija za uklanjanje poruke (FIFO - prva poruka koja je stigla)
Message* removeMessageFromWorker(Worker* worker) {
    std::lock_guard<std::mutex> lock(worker->mtx);

    if (worker->dataCount == 0) {
        //std::cerr << "Nema poruka za uklanjanje." << std::endl;
        return NULL;
    }

    // Prva poruka (FIFO)
    Message* removedMessage = worker->data[0];

    // Pomeramo sve ostale poruke unapred
    for (int i = 1; i < worker->dataCount; i++) {
        worker->data[i - 1] = worker->data[i];
    }

    worker->data[worker->dataCount - 1] = NULL; // Poslednji element sada je prazan
    worker->dataCount--;

    return removedMessage; // Korisnik mora osloboditi ovu memoriju nakon obrade
}

// Funkcija za uništavanje Workera i oslobađanje memorije
void destroyWorker(Worker* worker) {
    if (worker == nullptr) return;

    for (int i = 0; i < worker->dataCount; i++) {
        if (worker->data[i] != nullptr) {
            delete worker->data[i];  // Umesto free()
        }
    }

    delete[] worker->data;  // Umesto free()
    delete worker;          // Umesto free()
}

// Funkcija za ispis informacija o workeru
void printWorkerInfo(const Worker* worker) {
    printf("Worker ID: %d\n", worker->id);
    printf("Broj poruka: %d\n", worker->dataCount);
    printf("Maksimalan broj poruka: %d\n", MAX_DATA_SIZE);
    printf("Socket FD: %d\n", worker->socketFd);
    printf("Adresa workera: %u\n", worker->addr.sin_addr.s_addr);
}