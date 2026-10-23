#ifndef HISTORY_H_
#define HISTORY_H_

#include "editor.h"

typedef struct 
{
    Editor *editors;
    size_t size;
    size_t capacity;
    size_t index;
} History;

void history_append(History *history, Editor *editor);
bool history_undo(History *history, Editor *editor);
bool history_redo(History *history, Editor *editor);
void history_free(History *history);

#endif // HISTORY_H_