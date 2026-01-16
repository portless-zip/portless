#include "portless.h"

#ifdef _WIN32
#else
    #include <netinet/tcp.h>
    #include <netinet/in.h>
#endif

typedef struct { int fd; uint32_t id; } Stream;
Stream streams[MAX_STREAMS];
int tunnel_fd = -1;
uint32_t next_stream_id = 1;

int wa(int fd, const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = write(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

void sf(int fd, uint32_t sid, uint8_t type, uint32_t len, void* data) {
    portless_header_t h = { htonl(sid), type, htonl(len) };
    if (wa(fd, (char*)&h, sizeof(h)) < 0) return;
    if (len > 0 && data != NULL) wa(fd, (char*)data, len);
}

int main() {
    for(int i=0; i<MAX_STREAMS; i++) streams[i].fd = -1;
    int t_list = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; 
    setsockopt(t_list, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(36008) };
    bind(t_list, (struct sockaddr*)&addr, sizeof(addr));
    listen(t_list, 1);
    printf("[Relay] Started\n");
    tunnel_fd = accept(t_list, NULL, NULL);
    setsockopt(tunnel_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(int));
    printf("[Relay] Client connected to relay\n");

    int p_list = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(p_list, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    addr.sin_port = htons(25565);
    bind(p_list, (struct sockaddr*)&addr, sizeof(addr));
    listen(p_list, 10);

    struct pollfd fds[MAX_STREAMS + 2];
    while (1) {
        fds[0].fd = tunnel_fd; fds[0].events = POLLIN;
        fds[1].fd = p_list;    fds[1].events = POLLIN;
        int active_fds = 2;
        for (int i=0; i<MAX_STREAMS; i++) if(streams[i].fd != -1) { fds[active_fds].fd = streams[i].fd; fds[active_fds].events = POLLIN; active_fds++; }

        if (poll(fds, active_fds, -1) < 0) continue;

        if (fds[1].revents & POLLIN) {
            int p_fd = accept(p_list, NULL, NULL);
            setsockopt(p_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(int));
            for (int i=0; i<MAX_STREAMS; i++) if (streams[i].fd == -1) {
                streams[i].fd = p_fd; streams[i].id = next_stream_id++;
                sf(tunnel_fd, streams[i].id, FRAME_STREAM_OPEN, 0, NULL);
                printf("[Relay] Stream %u Opened\n", streams[i].id); break;
            }
        }

        if (fds[0].revents & POLLIN) {
            portless_header_t h;
            int r = recv(tunnel_fd, (char*)&h, sizeof(h), MSG_WAITALL);
            if (r <= 0) break;
            
            uint32_t sid = ntohl(h.stream_id);
            uint32_t len = ntohl(h.payload_length);

            if (h.frame_type == FRAME_STREAM_DATA && len > 0) {
                char* buf = malloc(len); 
                recv(tunnel_fd, buf, len, MSG_WAITALL);
                for(int i=0; i<MAX_STREAMS; i++) if (streams[i].id == sid && streams[i].fd != -1) wa(streams[i].fd, buf, len);
                free(buf);
            } else if (h.frame_type >= FRAME_STREAM_CLOSE) {
                for(int i=0; i<MAX_STREAMS; i++) if (streams[i].id == sid && streams[i].fd != -1) { close(streams[i].fd); streams[i].fd = -1; }
            }
        }

        for (int i=2; i<active_fds; i++) {
            if (fds[i].revents & POLLIN) {
                char buf[MAX_PAYLOAD]; int n = read(fds[i].fd, buf, MAX_PAYLOAD);
                uint32_t sid = 0;
                for(int j=0; j<MAX_STREAMS; j++) if(streams[j].fd == fds[i].fd) sid = streams[j].id;
                if (n <= 0) { sf(tunnel_fd, sid, FRAME_STREAM_CLOSE, 0, NULL); close(fds[i].fd); for(int j=0; j<MAX_STREAMS; j++) if(streams[j].fd == fds[i].fd) streams[j].fd = -1; }
                else sf(tunnel_fd, sid, FRAME_STREAM_DATA, n, buf);
            }
        }
    }
    return 0;
}