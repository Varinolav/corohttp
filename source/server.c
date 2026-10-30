#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>


void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    char buf[1024];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(8080);
    bind(fd, (struct sockaddr*) &addr, sizeof(addr));
    
    set_nonblocking(fd);

    if (listen(fd, 5) < 0) {
        perror("listen failed");
        close(fd);
        return 1;
    }

    for (;;) {
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept failed");
        close(fd);
        return 1;
    }
    set_nonblocking(client_fd);


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
                    "HTTP/1.1 200 OK\n"
                    "Content-Type: application/json; charset=utf-8\n"
                    "Content-Length: %d\n"
                    "Connection: close\n"
                    "\r\n"
                    "%s", 
                    body_len, json_body);

                write(client_fd, response, strlen(response));
                
            } else {
                const char *not_found = 
                    "HTTP/1.1 404 Not Found\n"
                    "Content-Length: 0\n"
                    "Connection: close\n\n";
                write(client_fd, not_found, strlen(not_found));
            }
        }
    }
    }

    close(fd);

}