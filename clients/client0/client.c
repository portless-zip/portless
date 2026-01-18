#include "portless.h"

stream_t streams[MAX_STREAMS];

int read_full(int fd, char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = read_socket(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

int write_full(int fd, const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = write_socket(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

int connect_to(const char* host, int port) {
    struct addrinfo hints, *res;
    char p[6]; snprintf(p, 6, "%d", port);
    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, p, &hints, &res) != 0) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0 || connect(fd, res->ai_addr, res->ai_addrlen) < 0) { freeaddrinfo(res); return -1; }
    freeaddrinfo(res); return fd;
}

void send_frame(int fd, uint32_t sid, uint8_t type, uint32_t len, void* data) {
    portless_header_t h = { htonl(sid), type, htonl(len) };
    write_full(fd, (char*)&h, sizeof(h));
    if (len > 0 && data != NULL) write_full(fd, (char*)data, len);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: client <port> <relay_ip>\n");
        printf("Example: client 25565 5.104.252.238\n");
        return 1;
    }
    int local_port = atoi(argv[1]);
    const char* relay_ip = argv[2];
    uint64_t total_rx = 0;

#ifdef _WIN32
    WSADATA w; WSAStartup(MAKEWORD(2,2), &w);
#endif

    while (1) {
        for(int i=0; i<MAX_STREAMS; i++) streams[i].fd = -1;
        int tunnel_fd = connect_to(relay_ip, 36008);
        if (tunnel_fd < 0) { printf("[Client] Relay %s offline. Retrying...\n", relay_ip); sleep(5); continue; }
        
        int opt = 1; setsockopt(tunnel_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
        printf("[Client] Connected to %s. Tunneling to 127.0.0.1:%d\n", relay_ip, local_port);

        struct pollfd fds[MAX_STREAMS + 1];
        while (1) {
            fds[0].fd = tunnel_fd; fds[0].events = POLLIN;
            int active = 1;
            for (int i=0; i<MAX_STREAMS; i++) if(streams[i].fd != -1) { fds[active].fd = streams[i].fd; fds[active].events = POLLIN; active++; }
            
            if (poll(fds, active, -1) < 0) break;

            if (fds[0].revents & POLLIN) {
                portless_header_t h;
                if (read_full(tunnel_fd, (char*)&h, sizeof(h)) < 0) break;
                uint32_t sid = ntohl(h.stream_id);
                uint32_t len = ntohl(h.payload_length);

                if (h.frame_type == FRAME_STREAM_OPEN) {
                    int l_fd = connect_to("127.0.0.1", local_port);
                    if (l_fd == -1) send_frame(tunnel_fd, sid, FRAME_STREAM_RESET, 0, NULL);
                    else { 
                        setsockopt(l_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
                        for(int i=0; i<MAX_STREAMS; i++) if(streams[i].fd == -1) { 
                            streams[i].fd = l_fd; 
                            streams[i].id = sid; 
                            streams[i].bytes_in = 0;
                            streams[i].bytes_out = 0;
                            break; 
                        }
                        printf("[Stream %u] Connected to 127.0.0.1:%d\n", sid, local_port);
                    }
                } else if (len > 0) {
                    char* buf = malloc(len); read_full(tunnel_fd, buf, len);
                    for(int i=0; i<MAX_STREAMS; i++) if (streams[i].id == sid && streams[i].fd != -1) {
                        write_full(streams[i].fd, buf, len);
                        streams[i].bytes_out += len;
                    }
                    total_rx += len;
                    if (total_rx % (1024*100) < len) printf("[Status] Total Data Received: %lu KB\n", total_rx/1024);
                    free(buf);
                }
                if (h.frame_type >= FRAME_STREAM_CLOSE) {
                    for(int i=0; i<MAX_STREAMS; i++) if (streams[i].id == sid && streams[i].fd != -1) { close(streams[i].fd); streams[i].fd = -1; }
                }
            }

            for (int i=1; i<active; i++) {
                if (fds[i].revents & POLLIN) {
                    char buf[MAX_PAYLOAD]; int n = read_socket(fds[i].fd, buf, MAX_PAYLOAD);
                    uint32_t sid = 0;
                    for(int j=0; j<MAX_STREAMS; j++) if((int)streams[j].fd == (int)fds[i].fd) sid = streams[j].id;
                    if (n <= 0) { send_frame(tunnel_fd, sid, FRAME_STREAM_CLOSE, 0, NULL); close(fds[i].fd); for(int j=0; j<MAX_STREAMS; j++) if((int)streams[j].fd == (int)fds[i].fd) streams[j].fd = -1; }
                    else send_frame(tunnel_fd, sid, FRAME_STREAM_DATA, n, buf);
                }
            }
        }
        close(tunnel_fd);
    }
    return 0;
}