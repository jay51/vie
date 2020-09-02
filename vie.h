#include<time.h>
#include<stdarg.h>
#include<stdio.h>
#ifndef VIE_H
#define VIE_H


typedef struct editor_row {
    int size;
    char* chars;
    int rsize;
    char* render;
} editor_row;

struct editor_config {
    int cx, cy, rx;
    int rows;
    int cols;
    int row_off;
    int col_off;
    int read_mod;
    int num_rows; /* num of rows in file */
    editor_row* row;
    struct termios orig_termios;
    char* filename;
    char statmsg[80];
    time_t statmsg_time;
} e_config;


void enable_raw_mode();
void disable_raw_mode();
void die(const char* err);

int  editor_read_key();
void editor_process_key(int chr);
void editor_refresh_screen();
void editor_draw_rows();
void init_editor();
int  get_win_size(int* rows, int* cols);
int  get_cursor_pos(int* rows, int* cols);
void editor_move_cursor(int c);
void editor_open(char* filename);
void editor_append_row(char* line, size_t len);
void editor_update_row(editor_row* row);
int editor_cx_to_rx(editor_row* row, int cx);
void editor_scroll();
void editor_draw_msg();
void editor_draw_statusbar();
void editor_row_insert_char(editor_row* row, int at, int c);
void editor_insert_char(int c);
char* editor_rows_To_String(int *buflen);
void editor_save();

void editor_set_statmsg(const char* fmt, ... ){
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e_config.statmsg, sizeof(e_config.statmsg), fmt, ap);
    va_end(ap);
    e_config.statmsg_time = time(NULL);
}
/* IO Buffer functions */
struct O_buff {
    char *b;
    int len;
} io_buff = {NULL, 0};

void buff_append(struct O_buff* buff, char* s){
    int len = strlen(s);
    char* ptr = realloc(buff->b, buff->len + len);
    if(ptr == NULL){
        perror("realloc");
        exit(1);
    }

    memcpy(&ptr[buff->len], s, len);
    buff->b = ptr;
    buff->len += len;
}

void buff_appendlen(struct O_buff* buff, char* s, int len){
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
    buff->b = NULL;
    buff->len = 0;
}
/* IO Buffer functions */
#endif
