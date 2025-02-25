#pragma once
#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

#include "worker.h"
#include "list.h"
#include <vector>
#include "queue.h"
#include "worker.h"

Worker* findMostFreeWorker(Node* workers);
void sendDataToWorker(Worker* worker, Queue* clientMessages);
void redistributeMessages(Queue* clientMessages, Node* workers);

	
#endif
