#pragma once
#ifndef MESSAGE_MAP_H
#define MESSAGE_MAP_H

#include <stdlib.h>
#include "message.h"

#define INITIAL_CAPACITY 1024

typedef struct MsgEntry {
    int key;
    Message* value;
    struct MsgEntry* next;
} MsgEntry;

typedef struct MessageMap {
    MsgEntry** buckets;
    int capacity;
} MessageMap;

unsigned int hash(int key, int capacity);
MessageMap* createMessageMap(int capacity);
void insertMessage(MessageMap* map, int key, Message* value);
Message* getMessage(MessageMap* map, int key);
int removeMessage(MessageMap* map, int key);
void freeMessageMap(MessageMap* map);

#endif