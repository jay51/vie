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
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include "vie.h"


#define VIE_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define VIE_TAB_STOP  8



// READ_KEY            = 3, // ctrl + c
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
    INSERT_KEY          = 'i',
    BACKSPACE           = 127,
    ENTER               = '\r',
};


int main(int argc, char** argv){
    enable_raw_mode();
    init_editor();
    if(argc >= 2)
        editor_open(argv[1]);

    editor_set_statmsg("HELP: Ctrl-Q = quit");

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
    e_config.row_off = 0;
    e_config.col_off = 0;
    e_config.statmsg_time = 0;
    e_config.statmsg[0] = '\0';
    e_config.cx = 0;
    e_config.cy = 0;
    e_config.rx = 0;
    e_config.row = NULL;
    e_config.filename = NULL;

    if(get_win_size(&e_config.rows, &e_config.cols) == -1)
        die("get_win_size");
    e_config.rows--; // last line will be the status bar
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


int editor_cx_to_rx(editor_row* row, int cx){
    int rx = 0;
    for(int j = 0; j < cx; j++){
        if(row->chars[j] == '\t')
            rx += (VIE_TAB_STOP - 1) - (rx % VIE_TAB_STOP);
        rx++;
    }
    return rx;
}

void editor_scroll(){
    e_config.rx = 0;
    if(e_config.cy < e_config.num_rows){
        e_config.rx = editor_cx_to_rx(&e_config.row[e_config.cy], e_config.cx);
    }

    // if y positon less than offset the file cursor will scroll from rows[offsite=0 + y] for scrolling up
    if(e_config.cy < e_config.row_off){
        e_config.row_off = e_config.cy;
    }

    // if y positon >= than offset the file cursor will scroll from rows[offsite + y] for scrolling down
    if(e_config.cy >= e_config.row_off + e_config.rows){
        e_config.row_off = e_config.cy - e_config.rows + 1;
    }

    if (e_config.rx < e_config.col_off) {
        e_config.col_off = e_config.rx;
    }
    if (e_config.rx >= e_config.col_off + e_config.cols) {
        e_config.col_off = e_config.rx - e_config.cols + 1;
    }
}

void editor_refresh_screen(){
    editor_scroll();
    // hide the cursor
    buff_append(&io_buff, "\x1b[?25l");
    // move curser to top
    buff_append(&io_buff, "\x1b[H");
    editor_draw_rows();
    if(strlen(e_config.statmsg))
        editor_draw_msg();
    else
        editor_draw_statusbar();

    char buf[32];
    // we never draw the cursor go down pass the screen; only the y positoin
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", e_config.cy - e_config.row_off + 1, e_config.rx - e_config.col_off + 1);
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

        case ENTER:
            e_config.cx = 0; // not usefull now
            break;

        case DEL_KEY:
        case BACKSPACE:
            // TODO:
            e_config.cx = 0; // not usefull now
            break;

        case CTRL_KEY('s'):
            editor_save();
            break;
        // Escape key. not usefulint buflenl
        case CTRL_KEY('l'):
        case '\x1b':
          break;

        default:
            if(e_config.read_mod){
                if(chr == INSERT_KEY)
                    e_config.read_mod = 0;
                else
                    editor_move_cursor(chr);

            } else if (chr == CTRL_KEY('c')){
                e_config.read_mod = 1;

            } else if (chr != 0 && chr >= 32 && chr <= 126){
                editor_insert_char(chr);
            }
            break;

    }
}


void editor_move_cursor(int c){
    // not sure if we need the ternery here
    editor_row* curr_row = (e_config.cy >= e_config.num_rows)? NULL:&e_config.row[e_config.cy];
    switch(c){
        case ARROW_UP:
            if(e_config.cy != 0)
                e_config.cy--;
            break;
        case ARROW_RIGHT:
                if(curr_row && e_config.cx < curr_row->size)
                    e_config.cx++;
            break;
        case ARROW_LEFT:
            if(e_config.cx != 0)
                e_config.cx--;
            break;
        case ARROW_DOWN:
            if(e_config.cy < e_config.num_rows)
                e_config.cy++;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = e_config.rows - 1;
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

    curr_row = (e_config.cy >= e_config.num_rows) ? NULL : &e_config.row[e_config.cy];
    int rowlen = curr_row ? curr_row->size : 0;
    if (e_config.cx > rowlen) {
        e_config.cx = rowlen;
    }
}


void editor_draw_rows(){
    int i;
    // buff[e_config.rows];
    for (i = 0; i < e_config.rows; i++){
        int filerow = e_config.row_off + i;
        if(filerow >= e_config.num_rows){
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
            int len = e_config.row[filerow].rsize - e_config.col_off;
            if(len < 0) len = 0;
            if(len > e_config.cols) len = e_config.cols;
            buff_appendlen(&io_buff, &e_config.row[filerow].render[e_config.col_off], len);
        }

        // clear each individual line (from active position to line end)
        buff_append(&io_buff, "\x1b[K");
        buff_append(&io_buff, "\r\n");
    }
}


void editor_draw_msg(){
    int msglen = strlen(e_config.statmsg);
    buff_append(&io_buff, "\x1b[K");
    if (msglen > e_config.cols) msglen = e_config.cols;
    if (msglen && time(NULL) - e_config.statmsg_time < 3)
        buff_appendlen(&io_buff, e_config.statmsg, msglen);
    else
        e_config.statmsg[0] = '\0';
}


void editor_draw_statusbar(){
    buff_append(&io_buff, "\x1b[7m"); // switch to inverted colors
    char status[80], rstatus[80];
    int len  = snprintf(status, sizeof(status), "%.20s - %dL",
        e_config.filename ? e_config.filename : "[No Name]", e_config.num_rows);

    int rlen  = snprintf(rstatus, sizeof(rstatus), "%d, %d", e_config.cy, e_config.cx);

    if (len > e_config.cols) len = e_config.cols;
        buff_appendlen(&io_buff, status, len);

    while(len < e_config.cols){
        if(e_config.cols - len == rlen){
            buff_appendlen(&io_buff, rstatus, rlen);
            break;
        }
        buff_append(&io_buff, " ");
        len++;
    }

    buff_append(&io_buff, "\x1b[m"); // switch inverted colors off
}

void editor_open(char* filename){
    e_config.filename = strdup(filename);

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
    e_config.row[at].rsize = 0;
    e_config.row[at].render = NULL;
    e_config.row[at].size = len;
    e_config.row[at].chars = malloc(sizeof(char) * len + 1);
    memcpy(e_config.row[at].chars, line, len);
    e_config.row[at].chars[len] = '\0';
    e_config.num_rows++;
    editor_update_row(&e_config.row[at]);
}

// convert all rows to a single long string sperated by \n
char* editor_rows_To_String(int *buflen){
    int len = 0;
    for(int i =0; i < e_config.num_rows; i++)
        len += e_config.row[i].size + 1;
    *buflen = len;

    char* buff = malloc(len);
    char* p = buff;
    for(int i =0; i < e_config.num_rows; i++){
        memcpy(p, e_config.row[i].chars, e_config.row[i].size);
        p += e_config.row[i].size;
        *p = '\n';
        p++;
    }
    return buff;
}


void editor_save(){
    if(e_config.filename == NULL) return;

    int len;
    char* buff = editor_rows_To_String(&len);
    int fd = open(e_config.filename, O_RDWR | O_CREAT, 0644);
    if(fd != -1){
        // resize the file
        if(ftruncate(fd, len) != -1){
            if(write(fd, buff, len) == len){
                close(fd);
                free(buff);
                editor_set_statmsg("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buff);
    editor_set_statmsg("Can't save! I/O error: %s", strerror(errno));
}

/*
* This function modifies the chars displayed in the editor
* like replace tabs \t with sapces.
*/
void editor_update_row(editor_row* row){
    int tabs = 0;
    int j;
    // count the number of tabs to allocat enough memory
    for(j = 0; j < row->size; j++)
        if(row->chars[j] == '\t') tabs++;

    // 7 is the number of spaces replacing the 1 tab (VIE_TAB_STOP - 1) + 1
    row->render = malloc(row->size + tabs * (VIE_TAB_STOP - 1) + 1);

    int idx = 0;
    for(j = 0;j<row->size; j++){
        if(row->chars[j] == '\t'){
            row->render[idx++] = ' ';
            // idx % 8 != is to add 7 spaces; we could also just set idx to 0 at end of loop instead
            while(idx % VIE_TAB_STOP != 0) row->render[idx++] = ' ';
        }
        else
            row->render[idx++] = row->chars[j];
    }

    row->render[idx] = '\0';
    row->rsize = idx;
}


/*
* If E.cy == E.numrows, then the cursor is on the tilde line after the end of the file,
* so we need to append a new row to the file before inserting a character there.
* After inserting a character, we move the cursor forward so that the next character
* the user inserts will go after the character just inserted.
*/
void editor_insert_char(int c){
    if(e_config.cy == e_config.num_rows){
        editor_append_row("", 0);
    }

    editor_row_insert_char(
        &e_config.row[e_config.cy],
        e_config.cx,
        c
    );
    e_config.cx++;
}



void editor_row_insert_char(editor_row* row, int at, int c){
    if(at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->chars[at] = c;
    row->size++;
    editor_update_row(row);
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
