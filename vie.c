#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include "vie.h"



struct termios orig_termios;

int main(int argc, char** argv){
    char c;
    char quit_char = 'q';

    enable_raw_mode();
    while(1){
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");

        // if control char
        if(iscntrl(c)){
            printf("%d\r\n", c);
        }else{
            printf("%d ('%c') \r\n", c, c);
        }

        // exit while loop when type q
        if(c == quit_char) break;
        c = '\0';
    }

    return 0;
}

// you could replace most of code here with the function cfmakeraw
void enable_raw_mode(){
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");

    atexit(disable_raw_mode);
    struct termios raw = orig_termios;

    // IXON (so ctrl+q and ctrl+s gets translated as raw bytes)
    // ICRNL (to translate ctrl+m as `\n` instead of `\r`)
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // turn off `\r\n` which is emited by defulat by the terminal when translating `\n`
    raw.c_oflag &= ~(OPOST);
    // set char size to be 8bits per byte
    raw.c_cflag |= (CS8);
    // ECHO (so you don't see what you're typing)
    // ISIG (so ctrl+c and ctrl+z gets translated as raw bytes)
    // IEXTEN (so ctrl+v gets translated as raw bytes)
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // set the minimum number of bytes of input needed before read() can return.
    raw.c_cc[VMIN] = 0;
    // set the maximum amount of time to wait before read() returns. 100 ms or 1/10 of a sec
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

void disable_raw_mode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void die(const char* err){
    perror(err);
    exit(1);
}
