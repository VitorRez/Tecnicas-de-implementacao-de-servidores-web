#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define FILE_PATH "file.txt"
#define IMAGE_PATH "image.jpg"
int client_sockets[MAX_CLIENTS];

void serve_file(int client_socket, const char* file_path, const char* content_type){
    FILE* file = fopen(file_path, "rb");
    if (file == NULL){
        char *error_response = "HTTP/1.1 500 Internal Server Error\nContent-Type: text/plain\nContent-Length: 21\n\nError opening file.txt";
        send(client_socket, error_response, strlen(error_response), 0);
        exit(EXIT_FAILURE);
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

void handle_chat(int client_socket, char* request){
    const char *chat_message = strstr(request, "CHAT:") + strlen("CHAT:");
    printf("Chat message: %s\n", chat_message);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int other_client_socket = client_sockets[i];
        if (other_client_socket > 0 && other_client_socket != client_socket) {
            send(other_client_socket, chat_message, strlen(chat_message), 0);
        }
    }
}

void handle_client(int client_socket) {
    char buffer[4096];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';

        if (strstr(buffer, "GET /file.txt") != NULL) {
            serve_file(client_socket, FILE_PATH, "text/plain");
        } else if (strstr(buffer, "GET /image.png") != NULL) {
            serve_file(client_socket, IMAGE_PATH, "image/png");
        } else if (strstr(buffer, "CHAT:") != NULL){
            handle_chat(client_socket, buffer);
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
    int server_fd;
    fd_set master, read_fds;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int fdmax, activity, new_socket;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Falha ao fazer o bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Falha ao escutar");
        exit(EXIT_FAILURE);
    }

    FD_SET(server_fd, &master);
    fdmax = server_fd;

    printf("Aguardando conexões...\n");

    while(1){
        read_fds = master;

        activity = select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        if(activity < 0){
            perror("Erro na chamada select\n");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) == -1) {
                perror("Falha ao aceitar a conexão");
                exit(EXIT_FAILURE);
            }

            printf("Conexão aceita\n");

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }

            FD_SET(new_socket, &master);

            if (new_socket > fdmax) {
                fdmax = new_socket;
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_socket = client_sockets[i];
            if (client_socket > 0 && FD_ISSET(client_socket, &read_fds)) {
                
                handle_client(client_socket);
        
                FD_CLR(client_socket, &master);
                close(client_socket);
                client_sockets[i] = 0;
        
                printf("Conexão fechada\n");
            }
        }
    }
}