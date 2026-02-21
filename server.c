#define _GNU_SOURCE
#include <pthread.h>
#include <netdb.h>
#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON/cJSON.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include "polycomm.h"
#include <sys/types.h>
#include <assert.h>
#include "util.h"
#include "network.h"
#include "client.h"

static void broadcast_clients(void *buf, size_t len)
{
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i<client_n; i++) {
        send_all(clients[i]->fd, buf, len);
    }
    pthread_mutex_unlock(&clients_mutex);
}

void disconnect_client(Client *client)
{
    if (!client) return;

    pthread_mutex_lock(&clients_mutex);

    // some magic stuff with removing the element only god knows how this works
    for (int i = 0; i < client_n; i++) {
        if (clients[i] == client) {
            clients[i] = clients[client_n - 1];
            clients[client_n - 1] = NULL;
            client_n--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    printf("Disconnecting client %s\n", client->username);

    cJSON *left = cJSON_CreateObject();
    char left_msg[64];
    snprintf(left_msg, sizeof(left_msg), "%s left", client->username);
    cJSON_AddStringToObject(left, "message", left_msg);
    cJSON_AddStringToObject(left, "author", "[SERVER]");
    char *pr = cJSON_PrintUnformatted(left);

    uint32_t length = strlen(pr);
    uint32_t net_length = htonl(strlen(pr));
    broadcast_clients(&net_length, 4);
    broadcast_clients(pr, length);


    if (client->username) {
        free(client->username);
        client->username = NULL;
    }

    close(client->fd);
    free(client);
    if(pr) {
        free(pr);
    }
    cJSON_Delete(left);
}

void *client_manage(void *arg)
{
    Client *client = (Client *) arg;
    int client_fd = client->fd;

    while(1) {
        uint32_t net_length_u;
        ssize_t n = recv_all(client_fd, &net_length_u, 4);
        if(n <= 0) { // n == 0 can ONLY mean disconnect for some reason??
            disconnect_client(client);
            return NULL;
        }

        uint32_t net_length = ntohl(net_length_u);

        if(net_length < 1048576) { // <--- thats one MB
            char *json = (char *) malloc(net_length+1);
            if(!json) continue;
            json[net_length] = '\0';

            if(recv_all(client_fd, json, net_length) == 0) {
                disconnect_client(client);
                return NULL;
            }

            cJSON *parsed_json = cJSON_Parse(json);
            if(!parsed_json) {
                free(json);
                continue;
            }

            cJSON *message = cJSON_GetObjectItemCaseSensitive(parsed_json, "message");
            cJSON *request = cJSON_GetObjectItemCaseSensitive(parsed_json, "request");
            if(message && strcmp(message->valuestring, "")) {
                cJSON_AddStringToObject(parsed_json, "author", client->username);

                char *pr_j = cJSON_PrintUnformatted(parsed_json);
                printf("%s: %s\n", client->username, message->valuestring);

                uint32_t length = strlen(pr_j);
                uint32_t net_length_j = htonl(strlen(pr_j));
                broadcast_clients(&net_length_j, 4);
                broadcast_clients(pr_j, length);
                if(pr_j) {
                    free(pr_j);
                }
            } else if(request && strcmp(request->valuestring, "")) {
                cJSON *username = cJSON_GetObjectItemCaseSensitive(parsed_json, "target");
                if(!username) {
                    if(json) {
                        free(json);
                        cJSON_Delete(parsed_json);
                    }
                    continue;
                }

                int found = 0;

                pthread_mutex_lock(&clients_mutex);
                for(int i = 0; i<client_n; i++) {
                    if(strcmp(clients[i]->username, username->valuestring) == 0) {
                        found = 1;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                if(found == 1) {
                    continue;
                }

                cJSON *changed = cJSON_CreateObject();
                char change_msg[128];
                snprintf(change_msg, sizeof(change_msg), "%s changed their username to %s", client->username, username->valuestring);
                cJSON_AddStringToObject(changed, "message", change_msg);
                cJSON_AddStringToObject(changed, "author", "[SERVER]");
                char *pr = cJSON_PrintUnformatted(changed);

                uint32_t length = strlen(pr);
                uint32_t net_length = htonl(strlen(pr));
                broadcast_clients(&net_length, 4);
                broadcast_clients(pr, length);

                if(pr) free(pr);
                cJSON_Delete(changed);
                free(client->username);
                client->username = strdup(username->valuestring);
            }
            cJSON_Delete(parsed_json);
            free(json);
        } else {
            puts("Max limit (1 MB) per receive exceeded, rejecting.");
        }
    }

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
            pthread_t client_thread;
            Client *client = malloc(sizeof(Client));
            strncpy(client->ip, client_ip, INET_ADDRSTRLEN);
            client->fd = client_fd;
            init_client(client);
            printf("Client accepted from %s with username %s.\n", client_ip, client->username);

            cJSON *joined = cJSON_CreateObject();
            char join_msg[64];
            snprintf(join_msg, sizeof(join_msg), "%s joined", client->username);
            cJSON_AddStringToObject(joined, "message", join_msg);
            cJSON_AddStringToObject(joined, "author", "[SERVER]");
            char *pr = cJSON_PrintUnformatted(joined);

            uint32_t length = strlen(pr);
            uint32_t net_length = htonl(strlen(pr));
            broadcast_clients(&net_length, 4);
            broadcast_clients(pr, length);

            pthread_create(&client_thread, NULL, client_manage, client);
            pthread_detach(client_thread);
            if(pr) {
                free(pr);
            }
        }
    }
}


