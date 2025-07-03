#pragma once
#ifndef MESSAGE_H
#define MESSAGE_H

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)
#define closesocket(s)  close(s)
#endif

#define BUFFER_SIZE 256  

enum MessageType {
    TEXT_MESSAGE = 1,
    FILE_MESSAGE = 2,
    CONTROL_MESSAGE = 3,
    ERROR_MESSAGE = 4
};

typedef struct Message {
    int        msg_id;        
    int        type;          
    socket_t   clientSocket;  
    char       content[BUFFER_SIZE];
} Message;

int compareMessagesById(void* a, void* b);

#endif // MESSAGE_H