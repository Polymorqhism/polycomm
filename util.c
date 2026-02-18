#include <stdio.h>
#include "polycomm.h"

const char banner[188] =    "┌─┐┌─┐┬ ┬ ┬┌─┐┌─┐┌┬┐┌┬┐\n"
                            "├─┘│ ││ └┬┘│  │ │││││││\n"
                            "┴  └─┘┴─┘┴ └─┘└─┘┴ ┴┴ ┴\n";
int get_choice(void)
{
    printf("Are you the client or the server (1 - client; 2 - server)? ");
    char choice[4];
    fgets(choice, sizeof(choice), stdin);

    if(choice[0] == '1')
        return 1;
    else if(choice[0] == '2')
        return 2;

    return -1;
}


void display_banner(void)
{
    printf("\033[2J");
    printf("%s\n\n", banner);
}
