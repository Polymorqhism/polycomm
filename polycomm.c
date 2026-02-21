#include <stdio.h>
#include "polycomm.h"
#include "tcp.h"
#include "util.h"
#include <signal.h>

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
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
