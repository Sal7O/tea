#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"

#define LINE_MIN_CAPACITY 32
#define EDITOR_MIN_CAPACITY 2

static void line_stretch(Line *line, size_t n);
static void line_insert_text_at_cursor_sized(Line *line, const char *text, size_t text_size, size_t *cursor);
static void line_insert_text_at_cursor(Line *line, const char *text, size_t *cursor);
static void line_backspace(Line *line, size_t *cursor);
static void fix_cursor_position(Editor *editor);

/////////////////////////////////////////////////////////////////

void editor_init(Editor *editor)
{
    editor->lines = malloc(sizeof(Line) * EDITOR_MIN_CAPACITY);
    editor->capacity = EDITOR_MIN_CAPACITY;
    for (size_t i = 0; i < editor->capacity; ++i) {
        editor->lines[i] = (Line) { 0 };
    }
    editor->size = 1;
    editor->cursor_row = 0;
    editor->cursor_col = 0;
}

void editor_insert_text(Editor *editor, const char *text)
{
    fix_cursor_position(editor);
    line_insert_text_at_cursor(&editor->lines[editor->cursor_row], text, &editor->cursor_col);
}

void editor_stretch(Editor *editor, size_t n)
{    
    bool capacity_changed = false;
    while (editor->capacity <= editor->size + n) {
        editor->capacity *= 2;
        capacity_changed = true;
    }

    if (capacity_changed) {
        editor->lines = realloc(editor->lines, sizeof(Line) * editor->capacity);
        if (!editor->lines) {
            fprintf(stderr, "Error: failed to stretch editor buffer. %s", strerror(errno));
        }
        for (size_t i = editor->size; i < editor->capacity; ++i) {
            editor->lines[i] = (Line) { 0 };
        }
    }
}

void editor_insert_new_line(Editor *editor)
{
    fix_cursor_position(editor);
    editor_stretch(editor, 1);

    Line *lines = editor->lines;
    size_t *row = &editor->cursor_row;
    size_t *col = &editor->cursor_col;
    memmove(lines + *row + 2, lines + *row + 1, (editor->size - *row - 1) * sizeof(Line));
    lines[*row + 1] = (Line) { 0 };

    size_t tmp_cursor = 0;
    line_insert_text_at_cursor_sized(lines + *row + 1, lines[*row].buf + *col, lines[*row].size - *col, &tmp_cursor);
    
    lines[*row].size = *col;
    editor->size += 1;
    *col = 0;
    *row += 1;
}

void editor_backspace(Editor *editor)
{
    if (editor->cursor_col == 0) {
        if (editor->cursor_row != 0) {
            size_t *row = &editor->cursor_row;
            Line *lines = editor->lines;

            *row -= 1;
            editor->cursor_col = lines[*row].size;
            size_t tmp_cursor = lines[*row].size;
            line_insert_text_at_cursor_sized(lines + *row, lines[*row + 1].buf, lines[*row + 1].size, &tmp_cursor);
            memmove(lines + *row + 1, lines + *row + 2, (editor->size - *row - 1) * sizeof(Line));
            editor->size -= 1;
        }
    } else {
        line_backspace(editor->lines + editor->cursor_row, &editor->cursor_col);
    }
}

void editor_left(Editor *editor)
{
    if (editor->cursor_col == 0) {
        if (editor->cursor_row != 0) {
            editor->cursor_row -= 1;
            editor->cursor_col = editor->lines[editor->cursor_row].size;
        }
    } else {
        editor->cursor_col -= 1;
    }
}

void editor_right(Editor *editor)
{
    if (editor->cursor_col == editor->lines[editor->cursor_row].size) {
        if (editor->cursor_row + 1 != editor->size) {
            editor->cursor_row += 1;
            editor->cursor_col = 0;
        }
    } else {
        editor->cursor_col += 1;
    }
}

void editor_up(Editor *editor)
{
    if (editor->cursor_row == 0) {
        editor->cursor_col = 0;
    } else {
        editor->cursor_row -= 1;
    }
}

void editor_down(Editor *editor)
{
    if (editor->cursor_row == editor->size - 1) {
        editor->cursor_col = editor->lines[editor->cursor_row].size;
    } else {
        editor->cursor_row += 1;
    }
}

//////////////////////////////////////////////////////////////

static void line_stretch(Line *line, size_t n)
{
    if (line->capacity == 0) {
        line->capacity = LINE_MIN_CAPACITY;
        line->buf = malloc(LINE_MIN_CAPACITY);
        if (!line->buf) {
            fprintf(stderr, "Error: failed to stretch line buffer. %s", strerror(errno));
        }
    }
    
    bool capacity_changed = false;
    while (line->capacity <= line->size + n) {
        line->capacity *= 2;
        capacity_changed = true;
    }

    if (capacity_changed) {
        line->buf = realloc(line->buf, line->capacity);
        if (!line->buf) {
            fprintf(stderr, "Error: failed to stretch line buffer. %s", strerror(errno));
        }
    }
}

static void line_insert_text_at_cursor_sized(Line *line, const char *text, size_t text_size, size_t *cursor)
{
    line_stretch(line, text_size);
    
    memmove(line->buf + *cursor + text_size, line->buf + *cursor, line->size - *cursor);
    memcpy(line->buf + *cursor, text, text_size);
    *cursor += text_size;
    line->size += text_size;
}

static void line_insert_text_at_cursor(Line *line, const char *text, size_t *cursor)
{
    size_t text_size = strlen(text);
    line_insert_text_at_cursor_sized(line, text, text_size, cursor);
}

static void line_backspace(Line *line, size_t *cursor)
{
    if (cursor == 0) {
        return;
    }

    memmove(line->buf + *cursor - 1, line->buf + *cursor, line->size - *cursor);
    *cursor -= 1;
    line->size -= 1;
}

static void fix_cursor_position(Editor *editor) 
{
    editor->cursor_col = 
        editor->cursor_col <= editor->lines[editor->cursor_row].size? 
        editor->cursor_col: editor->lines[editor->cursor_row].size;
}