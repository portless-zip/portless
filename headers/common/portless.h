#ifndef PORTLESS_H
#define PORTLESS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define poll WSAPoll
    #define close closesocket
    #define sleep(x) Sleep((x) * 1000)
    #define read_socket(fd, buf, len) recv(fd, buf, len, 0)
    #define write_socket(fd, buf, len) send(fd, buf, len, 0)
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <netdb.h>
    #include <poll.h>
    #include <unistd.h>
    #define read_socket(fd, buf, len) read(fd, buf, len)
    #define write_socket(fd, buf, len) write(fd, buf, len)
#endif

#define MAX_PAYLOAD 65536
#define MAX_STREAMS 64

#define FRAME_STREAM_OPEN  0x01
#define FRAME_STREAM_DATA  0x02
#define FRAME_STREAM_CLOSE 0x03
#define FRAME_STREAM_RESET 0x04

#pragma pack(push, 1)
typedef struct {
    uint32_t stream_id;
    uint8_t  frame_type;
    uint32_t payload_length;
} portless_header_t;
#pragma pack(pop)

typedef struct {
    int fd;
    uint32_t id;
    uint64_t bytes_in;
    uint64_t bytes_out;
} stream_t;

#endif