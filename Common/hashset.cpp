#include "hashset.h"
#include <stdlib.h>

HashNode* hashSet[HASH_TABLE_SIZE] = { 0 };

int hash(int value) {
    if (value < 0) value = -value;
    return value % HASH_TABLE_SIZE;
}

int hashset_contains(int value) {
    int idx = hash(value);
    HashNode* current = hashSet[idx];
    while (current) {
        if (current->value == value)
            return 1;
        current = current->next;
    }
    return 0;
}

void hashset_add(int value) {
    if (hashset_contains(value))
        return;

    int idx = hash(value);
    HashNode* newNode = (HashNode*)malloc(sizeof(HashNode));
    newNode->value = value;
    newNode->next = hashSet[idx];
    hashSet[idx] = newNode;
}

void hashset_free() {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode* current = hashSet[i];
        while (current) {
            HashNode* toDelete = current;
            current = current->next;
            free(toDelete);
        }
        hashSet[i] = NULL;
    }
}