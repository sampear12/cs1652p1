#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define BUFSIZE 4096
#define MAX_CLIENTS FD_SETSIZE

void handle_connection(int connfd) {
    char buffer[BUFSIZE];
    int recvd = recv(connfd, buffer, sizeof(buffer)-1, 0);
    if (recvd <= 0) { close(connfd); return; }
    buffer[recvd] = '\0';
    char method[16], path[256], protocol[16];
    if (sscanf(buffer, "%15s %255s %15s", method, path, protocol) != 3) { close(connfd); return; }
    if (strcmp(method, "GET") != 0) {
        const char *resp = "HTTP/1.0 501 Not Implemented\r\nContent-Type: text/plain\r\n\r\nMethod not implemented";
        send(connfd, resp, strlen(resp), 0);
        close(connfd); return;
    }
    char *filename = (path[0] == '/') ? path + 1 : path;
    if (strlen(filename) == 0) filename = "index.html";
    FILE *f = fopen(filename, "rb");
    if (!f) {
        const char *resp = "HTTP/1.0 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
        send(connfd, resp, strlen(resp), 0);
        close(connfd); return;
    }
    const char *header = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(connfd, header, strlen(header), 0);
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
        send(connfd, buffer, n, 0);
    fclose(f);
    close(connfd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    int port = atoi(argv[1]);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port) };
    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(listenfd, 10) < 0) { perror("listen"); exit(1); }

    int client_fds[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_fds[i] = -1;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenfd, &readfds);
        int maxfd = listenfd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > maxfd)
                    maxfd = client_fds[i];
            }
        }
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }
        if (FD_ISSET(listenfd, &readfds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int newfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
            if (newfd < 0) { perror("accept"); continue; }
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] == -1) { client_fds[i] = newfd; break; }
            }
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_fds[i];
            if (fd != -1 && FD_ISSET(fd, &readfds)) {
                handle_connection(fd);
                client_fds[i] = -1;
            }
        }
    }
    close(listenfd);
    return 0;
}
