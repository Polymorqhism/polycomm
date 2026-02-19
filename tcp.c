#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON/cJSON.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include "polycomm.h"

static void *handle_chat(void *arg)
{
    int client_fd = *(int *)arg;

    while(1) {
        char buf[INPUT_MAX];
        fgets(buf, INPUT_MAX-2, stdin);
        buf[strcspn(buf, "\n")] = 0;
        cJSON *message = cJSON_CreateObject();
        if(cJSON_AddStringToObject(message, "message", buf) == NULL) {
            return NULL;
        }
        char *pr = cJSON_PrintUnformatted(message);

        printf("[%s]: %s", buf, pr);
        send(client_fd, pr, strlen(pr), 0);

        cJSON_free(pr);
        cJSON_Delete(message);
    }
    return NULL;
}


void *client_manage(void *arg)
{

    return NULL;
}

void handle_server_choice(void)
{
    char port[8];
    printf("Enter a port: ");
    fgets(port, sizeof(port), stdin);

    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed.");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed.");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    uint16_t port_int = atoi(port);
    address.sin_port = htons(port_int);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed.");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listening failed.");
        exit(EXIT_FAILURE);
    }

    while(1) {
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen))< 0) {
            perror("Accepting failed.");
            exit(EXIT_FAILURE);
        } else {
            char client_ip[INET_ADDRSTRLEN];
            socklen_t addrlen = sizeof(addrlen);
            inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
            printf("Client accepted from %s.\n", client_ip);
        }
    }
}



void handle_client_choice(void)
{
    char ip[32];
    char port[8];
    printf("Enter server IP: ");
    fgets(ip, sizeof(ip), stdin);
    printf("Enter server port: ");
    fgets(port, sizeof(port), stdin);
    ip[strcspn(ip, "\n")] = 0;
    port[strcspn(port, "\n")] = 0;
    printf("\nIP set as %s and port as %s.\n", ip, port);

    int client_fd;
    struct sockaddr_in server_addr;
    if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error.");
        return;
    }

    server_addr.sin_family = AF_INET;
    uint16_t port_int = atoi(port);
    server_addr.sin_port = htons(port_int);

    if(inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Bad IP.");
        return;
    }


    if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect.");
        return;
    }

    puts("Connection successful.");
    printf("\033[2J");

    int *cfd = (int *) malloc(sizeof(int));
    cfd = &client_fd;

    pthread_t chat_thread;
    pthread_create(&chat_thread, NULL, handle_chat, cfd);
    pthread_detach(chat_thread);

    while(1) {
        // handle receiving from server
    }
}
