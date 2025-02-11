#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFSIZE 4096

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
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port) };
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(sockfd, 10) < 0) { perror("listen"); exit(1); }
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (connfd < 0) { perror("accept"); continue; }
        handle_connection(connfd);
    }
    close(sockfd);
    return 0;
}
