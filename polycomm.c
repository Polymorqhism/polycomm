#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "polycomm.h"

int get_choice(void)
{
    int c;
    printf("Are you the client or the server (1 - client; 2 - server)? ");
    scanf("%1d", &c);

    if(c == 1 || c == 2) {
        return c;
    }

    return -1;
}

void display_banner(void)
{
    printf("\033[2J");
    printf("%s\n\n", banner);
}

void handle_client_choice(void)
{
    char ip[16];
    uint16_t port;
    printf("Enter server IP: ");
    scanf("%15s", ip);
    printf("Enter server port: ");
    scanf("%5hu", &port);

    printf("\nIP set as %s and port as %hu.\n", ip, port);

    int client_fd;
    struct sockaddr_in server_addr;
    if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error.");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Bad IP.");
        return;
    }


    if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect.");
        return;
    }

    puts("Connection successful.");
}

void handle_server_choice(void)
{
    uint16_t port;
    printf("Enter a port: ");
    scanf("%5hu", &port);

    int server_fd, new_socket;
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
    address.sin_port = htons(port);

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
