#pragma once
// worker.h
#ifndef WORKER_H
#define WORKER_H

#include <winsock2.h> 
#include <mutex>      
#include <iostream>    
#include "message.h"
#define BUFFER_SIZE 256
#define MAX_DATA_SIZE 1000 

typedef struct Worker {
    int id;                 
    int dataCount;          
    Message** data;         
    int socketFd;           
    struct sockaddr_in addr;
    std::mutex mtx;         
} Worker;

Worker* createWorker(int id);
bool addMessageToWorker(Worker* worker, const Message* newMessage);
Message* removeMessageFromWorker(Worker* worker);
void destroyWorker(Worker* worker);
void printWorkerInfo(const Worker* worker);

#endif // WORKER_H

