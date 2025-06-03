#pragma once
// worker.h
#ifndef WORKER_H
#define WORKER_H

#include <winsock2.h> // Za socket funkcionalnost
#include <mutex>       // Za std::mutex
#include <iostream>    // Za ispis
#include "message.h"
#define BUFFER_SIZE 256
#define MAX_DATA_SIZE 1000 // Globalna konstanta koja definiše maksimalnu veličinu podataka za sve workere

// Struktura za Workera
typedef struct Worker {
    int id;                 // ID workera
    int dataCount;          // Trenutni broj poruka
    Message** data;         // Dinamički alociran niz pokazivača na poruke
    int socketFd;           // Deskriptor soketa
    struct sockaddr_in addr;// Adresa workera
    std::mutex mtx;         // Mutex za zaštitu podataka
} Worker;

// Deklaracije funkcija za rad sa workerom
Worker* createWorker(int id);
bool addMessageToWorker(Worker* worker, const Message* newMessage);
Message* removeMessageFromWorker(Worker* worker);
void destroyWorker(Worker* worker);
void printWorkerInfo(const Worker* worker);

#endif // WORKER_H

