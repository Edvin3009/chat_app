#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN 12
#define PORT 5069
#define MAX_CLIENTS 99
#define BUFFER_SIZE 1024
#define BUFFER_MESSAGE_SIZE 1010


int main() {
    int server_fd, new_fd;
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;

    struct pollfd *pfds;
    nfds_t nfds;
    int nmsg, dupl;
    char **usernames = calloc(MAX_CLIENTS, MAX_NAME_LEN);

    char *buffer = malloc(BUFFER_MESSAGE_SIZE);
    ssize_t bytes_read;
    size_t msgsize;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    server_addr_len = sizeof(server_addr);

    if ((bind(server_fd, (struct sockaddr *) &server_addr, server_addr_len)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if ((listen(server_fd, MAX_CLIENTS)) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // initialize polling array
    pfds = calloc(MAX_CLIENTS, sizeof(struct pollfd));
    
    // add listening socket
    pfds[0].fd = server_fd;
    pfds[0].events = POLLIN;
    nfds = 1;

    while(1) {
        // perform polling
        nmsg = poll(pfds, nfds, -1);
        if (nmsg < 0) {
            perror("poll failed");
            continue;
        }

        // check sockets
        for (nfds_t i = 0; i < nfds; i++) {
            if (pfds[i].revents != 0) {
                if (pfds[i].revents & POLLIN) {     
                    if (pfds[i].fd == server_fd) {      // if listening socket, accept new connection
                        if (nfds < MAX_CLIENTS) {
                            new_fd = accept(server_fd, (struct sockaddr *) &server_addr, &server_addr_len);
                            pfds[nfds].fd = new_fd;
                            pfds[nfds].events = POLLIN;
                            nfds++;
                            printf("User connected\n");
                        }
                    } else if (usernames[i] == 0) { // if login message
                        bytes_read = read(pfds[i].fd, buffer, BUFFER_MESSAGE_SIZE - 1);
                        buffer[bytes_read] = '\0';
                        char *newline = strchr(buffer, '\n');
                        if (newline) *newline = '\0';
                        dupl = 0;
                        for (nfds_t j = 1; j < nfds; j++) {
                            if (usernames[j] && strcmp(usernames[j], buffer) == 0) {
                                send(pfds[i].fd, "DUP\n", 4, 0);
                                dupl = 1;
                                break;
                            }
                        }
                        if (dupl == 0) {
                            usernames[i] = strdup(buffer);
                            send(pfds[i].fd, "OK\n", 3, 0);
                        }
                    } else {     // else if socket received data, read and broadcast data from client
                        bytes_read = read(pfds[i].fd, buffer, BUFFER_MESSAGE_SIZE - 1);
                        if (bytes_read < 0) {
                            perror("read failed");
                            goto close;
                        }
                        if (bytes_read == 0) {
                            goto close;
                        }
                        buffer[bytes_read] = '\0';
                        // broadcast to all clients
                        printf("%s sent: %s", usernames[i], buffer);
                        for (nfds_t j = 1; j < nfds; j++) {
                            if (j != i) {
                                char nbuffer[bytes_read + MAX_NAME_LEN + 2];
                                msgsize = snprintf(nbuffer, BUFFER_SIZE, "%s: %s", usernames[i], buffer);
                                send(pfds[j].fd, nbuffer, msgsize, 0);
                            }
                        }
                    } 
                } else {        // else (in case of error/POLLERR or disconnection/POLLHUP), close socket and update polling array
                close:    
                    if (close(pfds[i].fd) < 0) {
                        perror("close failed");
                        exit(EXIT_FAILURE);
                    }
                    if (i != (nfds - 1)) {
                        pfds[i].fd = pfds[nfds-1].fd;
                        usernames[i] = usernames[nfds-1];
                    }
                    pfds[nfds-1].fd = 0;
                    pfds[nfds-1].events = 0;
                    usernames[nfds-1] = 0;
                    nfds--;
                    i--;        // to not skip any sockets after altering array
                }
            }
        }
    }

    for (nfds_t i = 0; i < nfds; i++) {
        close(pfds[i].fd);
    }

    free(buffer);
    free(pfds);
    return 0;

}
