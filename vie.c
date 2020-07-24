#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include "vie.h"


#define VIE_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

typedef struct editor_row {
    int size;
    char* chars;
} editor_row ;

struct editor_config {
    int cx, cy;
    int rows;
    int cols;
    int read_mod;
    int num_rows;
    editor_row* row;
    struct termios orig_termios;
} e_config;

enum editor_keys {
    ARROW_LEFT          = 'h',
    ARROW_RIGHT         = 'l',
    ARROW_UP            = 'k',
    ARROW_DOWN          = 'j',
    PAGE_UP             = 21,
    PAGE_DOWN           = 4,
    LINE_END            = 36,
    LINE_START          = 94,
    DEL_KEY             = 1000,
};


int main(int argc, char** argv){
    enable_raw_mode();
    init_editor();
    if(argc >= 2)
        editor_open(argv[1]);

    int c;
    while(1){
        editor_refresh_screen();
        c = editor_read_key();
        // printf("char is %d %c\n", c, c);
        editor_process_key(c);
    }

    return 0;
}

void init_editor(){
    e_config.read_mod = 1;
    e_config.num_rows = 0;
    e_config.cx = 0;
    e_config.cy = 0;
    e_config.row = NULL;

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
    buff_append(&io_buff, "\x1b[?25l");
    // move curser to top
    buff_append(&io_buff, "\x1b[H");
    editor_draw_rows();

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", e_config.cy+1, e_config.cx+1);
    buff_append(&io_buff, buf);

    // reveal cursor
    buff_append(&io_buff, "\x1b[?25h");
    write(STDOUT_FILENO, io_buff.b, io_buff.len);
    // char len[32];
    // snprintf(len, sizeof(len), "%ld", strlen(io_buff.b));
    // write(STDOUT_FILENO, len, strlen(len));
    buff_free(&io_buff);
}


int  editor_read_key(){
    char c = '\0';
    if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");

    // read multip char
    if(c == '\x1b'){
        char seq[3];
        if(read(STDIN_FILENO, &seq[0], 1) == -1 && errno != EAGAIN) return c;
        if(read(STDIN_FILENO, &seq[1], 1) == -1 && errno != EAGAIN) return c;
        if (seq[0] == '[') {

            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return c;

                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '3': return DEL_KEY;
                    }
                }
            }

            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
    }
    return c;
}


void editor_process_key(int chr){
    // printf("char is %d %c\n", chr, chr);
    switch(chr){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 4);
            exit(0);
            break;

        case 'k':
        case 'l':
        case 'h':
        case 'j':
        case 4:
        case 21:
        case 36:
        case 94:
            if(e_config.read_mod){
                editor_move_cursor(chr);
            } else {
                // buff_append(&io_buff, &chr);
            }
            break;

        case 'i':
            e_config.read_mod = 0;
            break;
        case DEL_KEY:
            e_config.cx = 0; // not usefull now
            break;
    }
}


void editor_move_cursor(int c){
    switch(c){
        case ARROW_UP:
            if(e_config.cy != 0)
                e_config.cy--;
            break;
        case ARROW_RIGHT:
            if(e_config.cx != e_config.cols - 1)
                e_config.cx++;
            break;
        case ARROW_LEFT:
            if(e_config.cx != 0)
                e_config.cx--;
            break;
        case ARROW_DOWN:
            if(e_config.cy != e_config.rows - 1)
                e_config.cy++;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = e_config.rows;
                while (times--)
                    editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case LINE_END:
            e_config.cx = e_config.cols - 1;
            break;
        case LINE_START:
            e_config.cx = 0;
            break;
    }
}


void editor_draw_rows(){
    int i;
    // buff[e_config.rows];
    for (i = 0; i < e_config.rows; i++){
        if(i >= e_config.num_rows){
            if(e_config.num_rows == 0 && i == e_config.rows / 3){
                char welcome[80];
                int msg_size = snprintf(welcome, sizeof(welcome), "vieo editor -- version %s", VIE_VERSION);
                if(msg_size > e_config.cols)
                    msg_size = e_config.cols;

                // center msg
                int padding = (e_config.cols - msg_size) / 2;
                if(padding){
                    buff_append(&io_buff, "~");
                    padding--;
                }
                while(padding--)
                    buff_append(&io_buff, " ");

                buff_append(&io_buff, welcome);
            } else
                buff_append(&io_buff, "~");

        } else {
            int len = e_config.row[i].size;
            if(len > e_config.cols) len = e_config.cols;
            buff_appendlen(&io_buff, e_config.row[i].chars, len);
        }

        // clear each individual line (from active position to line end)
        buff_append(&io_buff, "\x1b[K");
        if (i < e_config.rows - 1)
            buff_append(&io_buff, "\r\n");
    }
}


void editor_open(char* filename){
    FILE* fp = fopen(filename, "r");
    if(!fp) die("fopen");

    char* line = NULL;
    size_t cap = 0;
    ssize_t len;

    while((len = getline(&line, &cap, fp)) != -1){
        // strip out \n and \r
        while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            len--;
        editor_append_row(line, len);
    }
    free(line);
    fclose(fp);
}

void editor_append_row(char* line, size_t len){
    e_config.row = realloc(e_config.row, sizeof(editor_row) * (e_config.num_rows + 1));

    int at = e_config.num_rows;
    e_config.row[at].size = len;
    e_config.row[at].chars = malloc(sizeof(char) * len + 1);
    memcpy(e_config.row[at].chars, line, len);
    e_config.row[at].chars[len] = '\0';
    e_config.num_rows++;
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


void die(const char* err){
    editor_refresh_screen();
    perror(err);
    exit(1);
}
