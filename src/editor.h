#ifndef EDITOR_H_
#define EDITOR_H_

#include <stddef.h>

typedef struct {
    char *buf;
    size_t size;
    size_t capacity;
} Line;

typedef struct {
    Line *lines;
    size_t size;
    size_t capacity;
    size_t cursor_row;
    size_t cursor_col;
} Editor;

void editor_init(Editor *editor);
void editor_left(Editor *editor);
void editor_right(Editor *editor);
void editor_up(Editor *editor);
void editor_down(Editor *editor);
void editor_backspace(Editor *editor);
void editor_insert_new_line(Editor *editor);
void editor_insert_text(Editor *editor, const char *text);

#endif // EDITOR_H_