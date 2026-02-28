#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <sys/select.h>

#define BUF_SIZE 8192

void usage() {
    printf("Usage: tcpproxy remote_host remote_port proxy_server_port\n");
}

int make_async(int s) {
    int flags;

    if ((flags = fcntl(s, F_GETFL)) == -1 ||
        fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    int n = 1;
    if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &n, sizeof(n)) == -1) {
        perror("setsockopt");
        return -1;
    }

    return 0;
}

int create_listener(char *port) {
    struct addrinfo hints, *res;
    int sock;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, port, &hints, &res);

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    bind(sock, res->ai_addr, res->ai_addrlen);

    listen(sock, 10);

    freeaddrinfo(res);

    return sock;
}

int connect_remote(char *host, char *port) {
    struct addrinfo hints, *res;
    int sock;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(host, port, &hints, &res);

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    connect(sock, res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res);

    return sock;
}

void proxy_loop(int client_fd, int server_fd) {

    char c2s_buf[BUF_SIZE];
    char s2c_buf[BUF_SIZE];

    int c2s_len = 0;
    int s2c_len = 0;

    while (1) {

        fd_set rset, wset;

        FD_ZERO(&rset);
        FD_ZERO(&wset);

        if (c2s_len < BUF_SIZE)
            FD_SET(client_fd, &rset);

        if (s2c_len < BUF_SIZE)
            FD_SET(server_fd, &rset);

        if (c2s_len > 0)
            FD_SET(server_fd, &wset);

        if (s2c_len > 0)
            FD_SET(client_fd, &wset);

        int maxfd = client_fd > server_fd ? client_fd : server_fd;

        if (select(maxfd + 1, &rset, &wset, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(client_fd, &rset)) {
            int n = read(client_fd, c2s_buf + c2s_len, BUF_SIZE - c2s_len);
            if (n <= 0)
                break;
            c2s_len += n;
        }

        if (FD_ISSET(server_fd, &wset)) {
            int n = write(server_fd, c2s_buf, c2s_len);
            if (n < 0)
                break;

            memmove(c2s_buf, c2s_buf + n, c2s_len - n);
            c2s_len -= n;
        }

        if (FD_ISSET(server_fd, &rset)) {
            int n = read(server_fd, s2c_buf + s2c_len, BUF_SIZE - s2c_len);
            if (n <= 0)
                break;
            s2c_len += n;
        }

        if (FD_ISSET(client_fd, &wset)) {
            int n = write(client_fd, s2c_buf, s2c_len);
            if (n < 0)
                break;

            memmove(s2c_buf, s2c_buf + n, s2c_len - n);
            s2c_len -= n;
        }
    }
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        usage();
        return 1;
    }

    char *remote_host = argv[1];
    char *remote_port = argv[2];
    char *proxy_port = argv[3];

    int listen_fd = create_listener(proxy_port);

    while (1) {

        int client_fd = accept(listen_fd, NULL, NULL);

        int server_fd = connect_remote(remote_host, remote_port);

        make_async(client_fd);
        make_async(server_fd);

        proxy_loop(client_fd, server_fd);

        close(client_fd);
        close(server_fd);
    }

    return 0;
}