#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

#define HISTORY_MIN_CAPACITY 2

static void history_stretch(History *history, size_t n);

void history_append(History *history, Editor *editor)
{
    history_stretch(history, 1);

    for (size_t i = history->index + 1; i < history->size; ++i) {
        editor_free(history->editors + i);
    }

    history->index += 1;
    history->size = history->index + 1;
    history->editors[history->index] = *editor;
}

bool history_undo(History *history, Editor *editor)
{
    if (history->index == 0) {
        return false; 
    }
    history->index -= 1;
    editor_free(editor);
    editor_deep_copy(&history->editors[history->index], editor);
    return true;
}

bool history_redo(History *history, Editor *editor)
{
    if (history->index + 1 >= history->size) {
        return false;
    }
    history->index += 1;
    editor_free(editor);
    editor_deep_copy(&history->editors[history->index], editor);
    return true;
}

void history_free(History *history)
{
    for (size_t i = 0; i < history->size; ++i) {
        editor_free(&history->editors[i]);
    }
    free(history->editors);
}

static void history_stretch(History *history, size_t n)
{
    if (history->capacity == 0) {
        history->capacity = HISTORY_MIN_CAPACITY;
        history->editors = malloc(sizeof(Editor) * HISTORY_MIN_CAPACITY);
        if (!history->editors) {
            fprintf(stderr, "Error: Failed to allocate memory. %s\n", strerror(errno));
            exit(1);
        }
    }

    bool changed = false;
    while (history->capacity < history->size + n) {
        changed = true;
        history->capacity *= 2;
    }

    if (changed) {
        history->editors = realloc(history->editors, sizeof(Editor) * history->capacity);
        if (!history->editors) {
            fprintf(stderr, "Error: Failed to allocate memory. %s\n", strerror(errno));
            exit(1);
        }
    }
}