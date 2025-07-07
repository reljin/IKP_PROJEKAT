#pragma once
#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

#include "worker.h"
#include "list.h"
#include <vector>
#include "queue.h"


Worker* findMostFreeWorker(Node* workers);
bool sendDataToWorker(Worker* worker, Queue* clientMessages);
void redistributeMessagesDead(Queue* clientMessages, Node* workers);

	
#endif
