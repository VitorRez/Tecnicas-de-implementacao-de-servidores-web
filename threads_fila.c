#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>

#define PORT 8002
#define MAX_THREADS 4
#define FILE_PATH "file.txt"
#define IMAGE_PATH "image.jpg"

typedef struct tasknode{
    int client_socket;
    struct tasknode* next;
}TaskNode;

TaskNode* task_queue = NULL;
TaskNode* tail = NULL;

sem_t empty, full;
pthread_mutex_t mutex;

void handle_error(const char* msg){
    perror(msg);
    exit(1);
}

void serve_file(int client_socket, const char* file_path, const char* content_type){
    FILE* file = fopen(file_path, "rb");
    if (file == NULL){
        perror("Falha ao abrir arquivo\n");
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

void enqueue_task(int client_socket){
    TaskNode* new_node = (TaskNode*)malloc(sizeof(TaskNode));
    new_node->client_socket = client_socket;
    new_node->next = NULL;

    if(task_queue == NULL){
        task_queue = new_node;
        tail = new_node;
    }else{
        tail->next = new_node;
        tail = new_node;
    }
}

int dequeue_task(){
    if(task_queue == NULL){
        return -1;
    }

    int client_socket = task_queue->client_socket;
    TaskNode* temp = task_queue;
    task_queue = task_queue->next;

    free(temp);
    return client_socket;
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

void* worker_thread(void* arg){
    while(1){
        sem_wait(&full);
        pthread_mutex_lock(&mutex);
        int client_socket = dequeue_task();
        pthread_mutex_unlock(&mutex);
        sem_post(&empty);

        if(client_socket != 1){
            handle_client(client_socket);
        }
    }
}

int main(){
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
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
        exit(EXIT_FAILURE);
    }
    

    if(listen(server_fd, 3) == -1){
        handle_error("Falha ao escutar");
        exit(EXIT_FAILURE);
    }

    sem_init(&empty, 0, MAX_THREADS);
    sem_init(&full, 0, 0);
    pthread_mutex_init(&mutex, NULL);

    pthread_t thread[MAX_THREADS];
    for(int i = 0; i < MAX_THREADS; i++){
        pthread_create(&thread[i], NULL, worker_thread, NULL);
    }

    while(1){
        printf("Aguardando conexões...\n");

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Falha ao aceitar a conexão");
            exit(EXIT_FAILURE);
        }

        printf("Conexão aceita\n");

        sem_wait(&empty);
        pthread_mutex_lock(&mutex);
        enqueue_task(new_socket);
        pthread_mutex_unlock(&mutex);
        sem_post(&full);
    }

    return 0;
}