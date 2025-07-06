#include "message_map.h"

unsigned int hash(int key, int capacity) {
    return (unsigned int)key % capacity;
}


MessageMap* createMessageMap(int capacity) {
    MessageMap* map = (MessageMap*)malloc(sizeof(MessageMap));
    if (!map) return NULL;

    map->capacity = capacity;
    map->buckets = (MsgEntry**)calloc(capacity, sizeof(MsgEntry*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    return map;
}


void insertMessage(MessageMap* map, int key, Message* value) {
    unsigned int idx = hash(key, map->capacity);

    MsgEntry* entry = map->buckets[idx];
    while (entry != NULL) {
        if (entry->key == key) {
            entry->value = value;  // overwrite
            return;
        }
        entry = entry->next;
    }

    MsgEntry* newEntry = (MsgEntry*)malloc(sizeof(MsgEntry));
    newEntry->key = key;
    newEntry->value = value;
    newEntry->next = map->buckets[idx];
    map->buckets[idx] = newEntry;
}


Message* getMessage(MessageMap* map, int key) {
    unsigned int idx = hash(key, map->capacity);
    MsgEntry* entry = map->buckets[idx];
    while (entry != NULL) {
        if (entry->key == key) return entry->value;
        entry = entry->next;
    }
    return NULL;
}


int removeMessage(MessageMap* map, int key) {
    unsigned int idx = hash(key, map->capacity);
    MsgEntry* curr = map->buckets[idx];
    MsgEntry* prev = NULL;

    while (curr != NULL) {
        if (curr->key == key) {
            if (prev) prev->next = curr->next;
            else map->buckets[idx] = curr->next;

            free(curr);
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}


void freeMessageMap(MessageMap* map) {
    for (int i = 0; i < map->capacity; ++i) {
        MsgEntry* entry = map->buckets[i];
        while (entry != NULL) {
            MsgEntry* temp = entry;
            entry = entry->next;
            free(temp);
        }
    }
    free(map->buckets);
    free(map);
}
