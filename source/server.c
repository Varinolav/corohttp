#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static void* handle_request(void* arg) {
    int client_fd  = (int)(intptr_t) arg;
    char buf[1024];

    ssize_t bytes_read = read(client_fd, buf, sizeof(buf) - 1);

    if (bytes_read > 0) {
        buf[bytes_read] = '\0'; 
        
        char method[16];
        char path[256];
        char version[16];
        char name[256];

        if (sscanf(buf, "%15s %255s %15s", method, path, version) == 3) {
            if (sscanf(path, "/hello/%255[^/?]", name) == 1) {
                
                char json_body[512];
                int body_len = snprintf(json_body, sizeof(json_body), "{\"message\":\"%s\"}", name);

                char response[1024];
                snprintf(response, sizeof(response),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json; charset=utf-8\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s", 
                    body_len, json_body);

                write(client_fd, response, strlen(response));
                
            } else {
                const char *not_found = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n\r\n";
                write(client_fd, not_found, strlen(not_found));
            }
        }
    }
    close(client_fd);
    return NULL;
}




static void accept_loop(int listen_fd) {

    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept failed");
            continue; 
        }

        pthread_t thread;
        int error = pthread_create(&thread, NULL, handle_request,
                                   (void*)(intptr_t)client_fd);
        if (error != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(error));
            close(client_fd);
            continue;
        }
        pthread_detach(thread);
    }
}




int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket failed");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(8080);
    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(fd);
        return 1;
    }
    
    if (listen(fd, 1024) < 0) {
        perror("listen failed");
        close(fd);
        return 1;
    }
    printf("server is active on: http://127.0.0.1:8080\n");
    accept_loop(fd);
    close(fd);
}
