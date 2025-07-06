#pragma once
#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

#include "worker.h"
#include "list.h"
#include <vector>
#include "queue.h"
#include "worker.h"

Worker* findMostFreeWorker(Node* workers);
bool sendDataToWorker(Worker* worker, Queue* clientMessages);
void redistributeMessages(Queue* clientMessages, Node* workers);
void redistributeMessagesDead(Queue* clientMessages, Node* workers);
extern std::atomic<bool> isFREE_QUEUE_ACTIVE;

	
#endif
