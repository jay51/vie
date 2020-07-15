#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <termios.h>
#include "vie.h"



struct termios orig_termios;

int main(int argc, char** argv){
    char c;
    char quit_char = 'q';

    enable_raw_mode();
    // exit while loop when hit end of file or type q
    while(read(STDIN_FILENO, &c, 1) == 1 && c != quit_char){
        // if control char
        if(iscntrl(c)){
            printf("%d\n", c);
        }else{
            printf("%d ('%c') \n", c, c);
        }
    }
    disable_raw_mode();
    return 0;
}

void enable_raw_mode(){
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(){
    // tcgetattr(STDIN_FILENO, &raw);
    // raw.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
