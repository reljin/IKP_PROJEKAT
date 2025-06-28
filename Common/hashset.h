#ifndef HASHSET_H
#define HASHSET_H

#define HASH_TABLE_SIZE 1031  

typedef struct HashNode {
    int value;
    struct HashNode* next;
} HashNode;

extern HashNode* hashSet[HASH_TABLE_SIZE];

int hash(int value);
int hashset_contains(int value);
void hashset_add(int value);
void hashset_free();

#endif