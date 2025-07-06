#pragma once
// worker.h
#ifndef WORKER_H
#define WORKER_H

#include "list.h"
#include <winsock2.h> 
#include <mutex>      
#include <iostream>    
#include "message_map.h"
#define BUFFER_SIZE 256
#define MAX_DATA_SIZE 1000 

typedef struct Worker {
    int id;                 
    int dataCount;          
    Message** data;         
    int socketFd;           
    struct sockaddr_in addr;
    std::mutex mtx;         
    int targetMsgCount;
    bool isNew;
    MessageMap* inflightMessages;
    
} Worker;

Worker* createWorker(int id);
bool addMessageToWorker(Worker* worker, const Message* newMessage);
Message* removeMessageFromWorker(Worker* worker);
Message* removeMessageFromWorkerByMessageId(Worker* worker, int msg_id);
void destroyWorker(Worker* worker);
void printWorkerInfo(const Worker* worker);
Worker* selectWorker(Node* workers);

#endif // WORKER_H

