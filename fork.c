#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PORT 8001
#define FILE_PATH "file.txt"
#define IMAGE_PATH "image.jpg"

void handle_error(const char* msg){
    perror(msg);
    exit(1);
}

void serve_file(int client_socket, const char* file_path, const char* content_type){
    FILE* file = fopen(file_path, "rb");
    if (file == NULL){
        perror("Falha ao abrir arquivo\n");
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* file_content = malloc(file_size);
    fread(file_content, 1, file_size, file);
    fclose(file);

    char response_header[1024];
    sprintf(response_header, "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %ld\n\n", content_type, file_size);

    send(client_socket, response_header, strlen(response_header), 0);

    send(client_socket, file_content, file_size, 0);

    free(file_content);
}

void handle_client(int client_socket) {
    char buffer[4096];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';

        if (strstr(buffer, "GET /file.txt") != NULL) {
            serve_file(client_socket, FILE_PATH, "text/plain");
        } else if (strstr(buffer, "GET /image.jpg") != NULL) {
            serve_file(client_socket, IMAGE_PATH, "image/jpg");
        } else {
            char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nHello, World!";
            send(client_socket, response, strlen(response), 0);
            printf("Resposta enviada\n");
        }
    }
    close(client_socket);
    printf("Conexão fechada\n");
}

int main(){
    int server_fd, new_socket;
    struct sockaddr_in address;
    int adrlen = sizeof(address);
    int option = 1;

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        handle_error("Falha ao criar o socket");
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1){
        handle_error("Falha ao fazer o bind");
    }

    if(listen(server_fd, 3) == -1){
        handle_error("Falha ao escutar");
    }

    while(1){
        printf("Aguardando conexões...\n");

        if((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&adrlen)) == -1){
            handle_error("Falha ao aceitar a conexão");
            exit(EXIT_FAILURE);
        }

        printf("Conexão aceita\n");

        pid_t pid = fork();
        if(pid < 0){
            perror("Falha ao criar o processo filho");
            exit(EXIT_FAILURE);
        }
        if(pid == 0){
            close(server_fd);
            handle_client(new_socket);
            exit(0);
        }
        if(pid > 0){
            close(new_socket);
            waitpid(pid, NULL, WNOHANG);
        }
    }

    return 0;
}