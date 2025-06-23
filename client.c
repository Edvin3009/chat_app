#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5069
#define BUFFER_SIZE 1024
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"


int main() {
    int client_fd;
    struct sockaddr_in server_addr;

    struct pollfd *pfds;
    nfds_t nfds;
    int nmsg;

    char *buffer = malloc(BUFFER_SIZE);
    ssize_t bytes_read;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if ((inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr)) < 0) {
        perror("address conversion failed");
        exit(EXIT_FAILURE);
    }

    if ((connect(client_fd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    // create polling array and insert stdin and socket
    pfds = calloc(2, sizeof(struct pollfd));

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = client_fd;
    pfds[1].events = POLLIN;

    nfds = 2;

    while(1) {
        // perform polling
        nmsg = poll(pfds, nfds, -1);
        if (nmsg < 0) {
            perror("poll failed");
            break;
        }

        // check stdin
        if (pfds[0].revents & POLLIN) {
            bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE);
            if (bytes_read < 0) {
                perror("read failed");
                break;
            }
            buffer[bytes_read] = '\0';
            if ((send(client_fd, buffer, bytes_read, 0)) < 0){
                perror("send failed");
                goto close;
            };
        }

        // check socket
        if (pfds[1].revents & POLLIN) {
            bytes_read = read(pfds[1].fd, buffer, BUFFER_SIZE);
            if (bytes_read < 0) {
                perror("read failed");
                goto close;    
            } else if (bytes_read > 0) {
                buffer[bytes_read - 1] = '\0';
                printf(ANSI_COLOR_YELLOW "%s" ANSI_COLOR_RESET "\n", buffer);
            }
        }
    }

    close:
        close(client_fd);
        free(buffer);
        free(pfds);
        return 0;
}