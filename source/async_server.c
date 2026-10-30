#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "coroed/api/task.h"
#include "coroed/api/io.h"
#include "coroed/sched/io.h"

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

TASK_DECLARE(accept_loop, void, arg);
TASK_DECLARE(handle_request, void, arg);

int task_accept(struct task* caller, int listen_fd) {
    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd >= 0) {
            return client_fd;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            io_wait(caller, listen_fd, EPOLLIN);
            continue;
        }
        return -1;
    }
}
TASK_DEFINE(handle_request, void, arg) {
    int client_fd  = (int)(intptr_t) arg;
    char buf[1024];

    ssize_t bytes_read = task_read(__self, client_fd, buf, sizeof(buf) - 1);

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
                    "HTTP/1.1 200 OK\n"
                    "Content-Type: application/json; charset=utf-8\n"
                    "Content-Length: %d\n"
                    "Connection: close\n"
                    "\r\n"
                    "%s", 
                    body_len, json_body);

                task_write(__self, client_fd, response, strlen(response));
                
            } else {
                const char *not_found = 
                    "HTTP/1.1 404 Not Found\n"
                    "Content-Length: 0\n"
                    "Connection: close\n\n";
                task_write(__self, client_fd, not_found, strlen(not_found));
            }
        }
    }
    close(client_fd);
}




TASK_DEFINE(accept_loop, void, arg) {
    int listen_fd = (int)(intptr_t)arg;

    for (;;) {
        int client_fd = task_accept(__self, listen_fd);
        if (client_fd < 0) {
            perror("accept failed");
            continue; 
        }

        set_nonblocking(client_fd);

        GO(handle_request, (void*)(intptr_t)client_fd);
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
    
    set_nonblocking(fd);

    if (listen(fd, 5) < 0) {
        perror("listen failed");
        close(fd);
        return 1;
    }
    printf("Listening on http://127.0.0.1:8080\n");
    tasks_init();
    
    
    tasks_submit(&accept_loop, (void*)(intptr_t)fd);
    
    tasks_start();
    tasks_wait(); 
    tasks_destroy();
}
