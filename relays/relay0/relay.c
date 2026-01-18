#include "portless.h"

#define MAX_PORTS 50

stream_t streams[MAX_STREAMS];
uint32_t next_stream_id = 1;

int port_listeners[MAX_PORTS];
int num_ports = 0;

int write_all(int fd, const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = write_socket(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

void send_frame(int fd, uint32_t sid, uint8_t type, uint32_t len, void* data) {
    portless_header_t h = { htonl(sid), type, htonl(len) };
    if (write_all(fd, (char*)&h, sizeof(h)) < 0) return;
    if (len > 0 && data != NULL) write_all(fd, (char*)data, len);
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    WSADATA w; WSAStartup(MAKEWORD(2,2), &w);
#endif

    if (argc < 2) {
        printf("Usage: %s <port1> <port2> ... <portN>\n", argv[0]);
        return 1;
    }

    for(int i=0; i<MAX_STREAMS; i++) streams[i].fd = -1;

    int t_list = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(36008) };
    bind(t_list, (struct sockaddr*)&addr, sizeof(addr));
    listen(t_list, 1);

    printf("[Relay] waiting for tunnel\n");
    int tunnel_fd = accept(t_list, NULL, NULL);
    int opt = 1; setsockopt(tunnel_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
    printf("[Relay] Linked!\n");
    for (int i = 1; i < argc && num_ports < MAX_PORTS; i++) {
        int p = atoi(argv[i]);
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in p_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(p) };
        
        if (bind(sock, (struct sockaddr*)&p_addr, sizeof(p_addr)) == 0) {
            listen(sock, 10);
            port_listeners[num_ports++] = sock;
            printf("[Relay] Listening for clients on port %d\n", p);
        } else {
            printf("[Relay] Failed to bind port %d\n", p);
            close(sock);
        }
    }

    struct pollfd fds[MAX_STREAMS + MAX_PORTS + 1];

    while (1) {
        fds[0].fd = tunnel_fd; fds[0].events = POLLIN;
        for (int i = 0; i < num_ports; i++) {
            fds[i + 1].fd = port_listeners[i];
            fds[i + 1].events = POLLIN;
        }

        int active_base = num_ports + 1;
        int current_active = active_base;
        for (int i=0; i<MAX_STREAMS; i++) {
            if(streams[i].fd != -1) { 
                fds[current_active].fd = streams[i].fd; 
                fds[current_active].events = POLLIN; 
                current_active++; 
            }
        }

        if (poll(fds, current_active, -1) < 0) break;
        if (fds[0].revents & POLLIN) {
            portless_header_t h;
            if (recv(tunnel_fd, (char*)&h, sizeof(h), MSG_WAITALL) <= 0) break;
            uint32_t sid = ntohl(h.stream_id);
            uint32_t len = ntohl(h.payload_length);
            if (h.frame_type == FRAME_STREAM_DATA) {
                char* buf = malloc(len); recv(tunnel_fd, buf, len, MSG_WAITALL);
                for(int i=0; i<MAX_STREAMS; i++) {
                    if (streams[i].id == sid && streams[i].fd != -1) {
                        write_all(streams[i].fd, buf, len);
                        streams[i].bytes_out += len;
                    }
                }
                free(buf);
            } else if (h.frame_type >= FRAME_STREAM_CLOSE) {
                for(int i=0; i<MAX_STREAMS; i++) {
                    if (streams[i].id == sid && streams[i].fd != -1) {
                        printf("[Stream %u] Disconnected\n", sid);
                        close(streams[i].fd); streams[i].fd = -1;
                    }
                }
            }
        }
        for (int i = 0; i < num_ports; i++) {
            if (fds[i + 1].revents & POLLIN) {
                int p_fd = accept(port_listeners[i], NULL, NULL);
                for (int j=0; j<MAX_STREAMS; j++) {
                    if (streams[j].fd == -1) {
                        streams[j].fd = p_fd; 
                        streams[j].id = next_stream_id++;
                        streams[j].bytes_in = 0; 
                        streams[j].bytes_out = 0;
                        send_frame(tunnel_fd, streams[j].id, FRAME_STREAM_OPEN, 0, NULL);
                        printf("[Stream %u] Client connected to relay\n", streams[j].id); 
                        break;
                    }
                }
            }
        }
        for (int i = active_base; i < current_active; i++) {
            if (fds[i].revents & POLLIN) {
                char buf[MAX_PAYLOAD]; 
                int n = read_socket(fds[i].fd, buf, MAX_PAYLOAD);
                uint32_t sid = 0;
                
                for(int j=0; j<MAX_STREAMS; j++) {
                    if((int)streams[j].fd == (int)fds[i].fd) sid = streams[j].id;
                }

                if (n <= 0) { 
                    send_frame(tunnel_fd, sid, FRAME_STREAM_CLOSE, 0, NULL); 
                    close(fds[i].fd); 
                    for(int j=0; j<MAX_STREAMS; j++) if((int)streams[j].fd == (int)fds[i].fd) streams[j].fd = -1; 
                } else { 
                    send_frame(tunnel_fd, sid, FRAME_STREAM_DATA, n, buf); 
                    for(int j=0; j<MAX_STREAMS; j++) if(streams[j].id == sid) streams[j].bytes_in += n; 
                }
            }
        }
    }
    return 0;
}