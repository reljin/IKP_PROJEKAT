#include "message.h"
#include <iostream>
#include <cstring> 
#include <string>

int compareMessagesById(void* a, void* b) {
    Message* m1 = (Message*)a;
    Message* m2 = (Message*)b;
    return (m1->msg_id == m2->msg_id) ? 0 : 1;
}

int extractMsgIdFromWorkerResponse(const std::string& message) {
    size_t start = message.find('|');
    if (start == std::string::npos) return -1;

    size_t end = message.find('|', start + 1);
    if (end == std::string::npos) return -1;

    std::string idStr = message.substr(start + 1, end - start - 1);
    try {
        return std::stoi(idStr);
    }
    catch (...) {
        return -1;
    }
}