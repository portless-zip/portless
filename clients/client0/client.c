#include "portless.h"

typedef struct { int fd; uint32_t id; } Stream;
Stream streams[MAX_STREAMS];
int rf(int fd, char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = read_socket(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

int wf(int fd, const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = write_socket(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

int ct(const char* host, int port) {
    struct addrinfo hints, *res;
    char p[6]; snprintf(p, 6, "%d", port);
    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, p, &hints, &res) != 0) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);
    return fd;
}

void sf(int fd, uint32_t sid, uint8_t type, uint32_t len, void* data) {
    portless_header_t h = { htonl(sid), type, htonl(len) };
    if (wf(fd, (char*)&h, sizeof(h)) < 0) return;
    if (len > 0 && data != NULL) wf(fd, (char*)data, len);
}

int main(int argc, char *argv[]) {
    int tp = 65534; // for fun :3
    if (tp < 1 || tp > 65535) tp = 65534;
    if (tp == TUNNEL_PORT) {
        printf("[System] Target port cannot be the same as tunnel port (%d). Exiting.\n", TUNNEL_PORT);
        return 1;
    }
    const char* rh = "5.104.252.238";
    if (argc >= 2) {
        tp = atoi(argv[1]);
    }
    if (argc >= 3) {
        rh = argv[2];
    }

    printf("[System] Portless Client Starting...\n");
    printf("[System] Local Target: 127.0.0.1:%d or localhost:%d\n", tp, tp);
    printf("[System] Relay Server: %s:%d\n", rh,TUNNEL_PORT);

#ifdef _WIN32
    WSADATA w; WSAStartup(MAKEWORD(2,2), &w);
#endif

    while (1) {
        for(int i=0; i<MAX_STREAMS; i++) streams[i].fd = -1;

        printf("[Client] Connecting to Relay...\n");
        int tunnel_fd = ct(rh, 36008);
        if (tunnel_fd < 0) { 
            printf("[Client] Offline. Retrying...\n");
            sleep(5); 
            continue; 
        }

        int opt = 1;
        setsockopt(tunnel_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(int));
        printf("[Client] Tunnel Established!\n");

        struct pollfd fds[MAX_STREAMS + 1];
        while (1) {
            memset(fds, 0, sizeof(fds));
            fds[0].fd = tunnel_fd; 
            fds[0].events = POLLIN;
            int active_fds = 1;
            for (int i=0; i<MAX_STREAMS; i++) {
                if(streams[i].fd != -1) { 
                    fds[active_fds].fd = streams[i].fd; 
                    fds[active_fds].events = POLLIN; 
                    active_fds++; 
                }
            }
            
            if (poll(fds, active_fds, -1) < 0) break;

            if (fds[0].revents & POLLIN) {
                portless_header_t h;
                if (rf(tunnel_fd, (char*)&h, sizeof(h)) < 0) break;
                
                uint32_t sid = ntohl(h.stream_id);
                uint32_t len = ntohl(h.payload_length);

                if (h.frame_type == FRAME_STREAM_OPEN) {
                    int l_fd = ct("127.0.0.1", tp); // only works on 127.0.0.1, not 0.0.0.0 3:
                    if (l_fd != -1) setsockopt(l_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(int));
                    
                    if (l_fd == -1) {
                        sf(tunnel_fd, sid, FRAME_STREAM_RESET, 0, NULL);
                    } else {
                        for(int i=0; i<MAX_STREAMS; i++) if(streams[i].fd == -1) { 
                            streams[i].fd = l_fd; streams[i].id = sid; break; 
                        }
                        printf("[Client] Stream %u connected to Local Port %d\n", sid, tp);
                    }
                } else {
                    if (len > 0) {
                        char* buf = malloc(len);
                        if (rf(tunnel_fd, buf, len) < 0) { free(buf); break; }
                        for(int i=0; i<MAX_STREAMS; i++) {
                            if (streams[i].id == sid && streams[i].fd != -1) {
                                if (h.frame_type == FRAME_STREAM_DATA) wf(streams[i].fd, buf, len);
                            }
                        }
                        free(buf);
                    }
                    if (h.frame_type >= FRAME_STREAM_CLOSE) {
                        for(int i=0; i<MAX_STREAMS; i++) if (streams[i].id == sid && streams[i].fd != -1) { 
                            close(streams[i].fd); streams[i].fd = -1; 
                        }
                    }
                }
            }

            for (int i=1; i<active_fds; i++) {
                if (fds[i].revents & POLLIN) {
                    char buf[MAX_PAYLOAD];
                    int n = read_socket(fds[i].fd, buf, MAX_PAYLOAD);
                    uint32_t sid = 0;
                    for(int j=0; j<MAX_STREAMS; j++) if(streams[j].fd == fds[i].fd) sid = streams[j].id;
                    
                    if (n <= 0) { 
                        sf(tunnel_fd, sid, FRAME_STREAM_CLOSE, 0, NULL); 
                        close(fds[i].fd); 
                        for(int j=0; j<MAX_STREAMS; j++) if(streams[j].fd == fds[i].fd) streams[j].fd = -1; 
                    }
                    else sf(tunnel_fd, sid, FRAME_STREAM_DATA, n, buf);
                }
            }
        }
        close(tunnel_fd);
    }
    return 0;
}