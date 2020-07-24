#ifndef VIE_H
#define VIE_H

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
