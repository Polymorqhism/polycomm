#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "polycomm.h"
#include "util.h"

void handle_chat(int client_fd)
{

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
    handle_chat(client_fd);
}

void handle_server_choice(void)
{
    char port[8];
    printf("Enter a port: ");
    fgets(port, sizeof(port), stdin);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        perror("Socket failed.");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
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
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen))< 0) {
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

int main(void)
{
    display_banner();

    int choice = get_choice();
    if(choice < 0) {
        puts("Invalid.");
        return 1;
    }

    if(choice == 1) {
        handle_client_choice();
    } else {
        handle_server_choice();
    }

    return 0;
}
