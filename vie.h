#ifndef VIE_H
#define VIE_H

void enable_raw_mode();
void disable_raw_mode();
void die(const char* err);

void editor_read_key(char* c);
void editor_process_key(char* c);
void editor_refresh_screen();
void editor_draw_rows();
void init_editor();
int  get_win_size(int* rows, int* cols);
int  get_cursor_pos(int* rows, int* cols);

#endif
