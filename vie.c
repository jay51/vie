#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include "vie.h"


#define VIE_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

struct editor_config {
    int rows;
    int cols;
    struct termios orig_termios;
} e_config;


/* IO Buffer functions */
struct O_buff {
    char *b;
    int len;
} io_buff = {NULL, 0};

void buff_append(struct O_buff* buff, char* s, int len){

    char* ptr = realloc(buff->b, buff->len + len);
    if(ptr == NULL){
        perror("realloc");
        exit(1);
    }

    memcpy(&ptr[buff->len], s, len);
    buff->b = ptr;
    buff->len += len;
}


void buff_free(struct O_buff* buff){
    free(buff->b);
}
/* IO Buffer functions */


int main(int argc, char** argv){
    char c;

    enable_raw_mode();
    init_editor();
    editor_refresh_screen();
    while(1){
        editor_read_key(&c);
        editor_process_key(&c);

        c = '\0';
    }

    return 0;
}

void init_editor(){
    if(get_win_size(&e_config.rows, &e_config.cols) == -1)
        die("get_win_size");
}

void enable_raw_mode(){
    if(tcgetattr(STDIN_FILENO, &e_config.orig_termios) == -1)
        die("tcgetattr");

    struct termios raw = e_config.orig_termios;
#ifdef __FreeBSD__
    cfmakeraw(&raw);
#else
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~OPOST;
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
#endif
    // set the minimum number of bytes of input needed before read() can return.
    raw.c_cc[VMIN] = 0;
    // set the maximum amount of time to wait before read() returns. 100 ms or 1/10 of a sec
    raw.c_cc[VTIME] = 1;

    atexit(disable_raw_mode);
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

void disable_raw_mode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &e_config.orig_termios) == -1)
        die("tcsetattr");
}


void editor_refresh_screen(){
    // hide the cursor
    buff_append(&io_buff, "\x1b[?25l", 6);
    // clear the screen
    // buff_append(&io_buff, "\x1b[2J", 4);
    // move curser to top
    buff_append(&io_buff, "\x1b[H", 3);
    editor_draw_rows();
    buff_append(&io_buff, "\x1b[H", 3);
    // reveal cursor
    buff_append(&io_buff, "\x1b[?25h", 6);
    // printf("buf is %s", io_buff.b);
    write(STDOUT_FILENO, io_buff.b, io_buff.len);
    buff_free(&io_buff);
}


void editor_read_key(char* c){
    if(read(STDIN_FILENO, c, 1) == -1 && errno != EAGAIN)
        die("read");
}


void editor_process_key(char* c){
    switch(*c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 4);
          exit(0);
          break;
    }
}


int get_win_size(int* rows, int* cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        // there's a bug here ( prints the char sequnce to termianl)
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_cursor_pos(rows, cols);
        // printf("rows %d, cols %d \n", *rows, *cols);

    } else{
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}





int get_cursor_pos(int* rows, int* cols){
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    char buf[32];
    unsigned int i = 0;

    while(i < sizeof(buf) -1){
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}


void editor_draw_rows(){
    int i;
    // buff[e_config.rows];
    for (i = 0; i < e_config.rows; i++){
        if(i == e_config.rows / 3){
            char welcome[80];
            int msg_size = snprintf(welcome, sizeof(welcome), "vieo editor -- version %s", VIE_VERSION);
            if(msg_size > e_config.cols)
                msg_size = e_config.cols;

            // center msg
            int padding = (e_config.cols - msg_size) / 2;
            if(padding){
                buff_append(&io_buff, "~", 1);
                padding--;
            }
            while(padding--)
                buff_append(&io_buff, " ", 1);

            buff_append(&io_buff, welcome, msg_size);
        } else{
            buff_append(&io_buff, "~", 1);
        }

        // clear each individual line (from active position to line end)
        buff_append(&io_buff, "\x1b[K", 3);

        if (i < e_config.rows - 1)
            buff_append(&io_buff, "\r\n", 2);
    }
}

void die(const char* err){
    editor_refresh_screen();
    perror(err);
    exit(1);
}
