#include <stdio.h>
#include <ncurses.h>
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


WINDOW *output_win;
WINDOW *input_win;

void init_ncurses(void)
{
        initscr();
        cbreak();
        keypad(stdscr, TRUE);

        if (has_colors()) {
            start_color();
            use_default_colors();
            init_pair(1, COLOR_CYAN, -1);
        }

        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        output_win = newwin(rows - 1, cols, 0, 0);
        input_win  = newwin(1, cols, rows - 1, 0);

        scrollok(output_win, TRUE);
        wmove(input_win, 0, 0);
        wrefresh(input_win);
        wrefresh(output_win);

}
