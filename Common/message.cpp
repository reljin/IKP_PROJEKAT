#include "message.h"
#include <iostream>
#include <cstring> 

int compareMessagesById(void* a, void* b) {
    Message* m1 = (Message*)a;
    Message* m2 = (Message*)b;
    return (m1->msg_id == m2->msg_id) ? 0 : 1;
}