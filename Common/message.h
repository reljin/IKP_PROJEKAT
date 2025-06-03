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

#define BUFFER_SIZE 256  // We keep 256 here to match your originals

/// Each Message carries:
///   - a unique msg_id (assigned by LB if client sends 0),
///   - a `type` (text vs file vs control),
///   - the `clientSocket` (so LB knows where to bounce the ACK),
///   - plus up to 256 bytes of `content`.
enum MessageType {
    TEXT_MESSAGE = 1,
    FILE_MESSAGE = 2,
    CONTROL_MESSAGE = 3,
    ERROR_MESSAGE = 4
};

typedef struct Message {
    int        msg_id;        // Unique ID (0 if client leaves it to LB)
    int        type;          // TEXT_MESSAGE, etc.
    socket_t   clientSocket;  // The socket of the original client
    char       content[BUFFER_SIZE];
} Message;

#endif // MESSAGE_H