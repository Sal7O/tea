#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <errno.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FPS 60
#define FRAME_TIME (1000 / (FPS))
#define SCALE 5.0f

#define UNUSED __attribute__((unused))

#if SDL_BYTEORDER == SDL_LIL_ENDIAN

#define GET_COLOR(color)    ((((color) >> (8 * 3)) & 0xff) << (8 * 0)) + \
                            ((((color) >> (8 * 2)) & 0xff) << (8 * 1)) + \
                            ((((color) >> (8 * 1)) & 0xff) << (8 * 2)) + \
                            ((((color) >> (8 * 0)) & 0xff) << (8 * 3))

#define UNHEXA(color)   (((color) >> (8 * 3)) & 0xff), \
                        (((color) >> (8 * 2)) & 0xff), \
                        (((color) >> (8 * 1)) & 0xff), \
                        (((color) >> (8 * 0)) & 0xff)

#define UNHEX(color)    ((color) >> (8 * 3)) & 0xff, \
                        ((color) >> (8 * 2)) & 0xff, \
                        ((color) >> (8 * 1)) & 0xff

#define GET_ALPHA(color) ((color) >> (8 * 0)) & 0xff

#else

#define GET_COLOR(color)    (color)

#define UNHEXA(color)   (((color) >> (8 * 0)) & 0xff), \
                        (((color) >> (8 * 1)) & 0xff), \
                        (((color) >> (8 * 2)) & 0xff), \
                        (((color) >> (8 * 3)) & 0xff)

#define UNHEX(color)    ((color) >> (8 * 0)) & 0xff, \
                        ((color) >> (8 * 1)) & 0xff, \
                        ((color) >> (8 * 2)) & 0xff

#define GET_ALPHA(color) ((color) >> (8 * 3)) & 0xff

#endif 

static void scc(int code)
{
    if (code < 0) {
        fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
        exit(1);
    }
}

static void *scp(void *ptr)
{
    if (!ptr) {
        fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
        exit(1);
    }
    return ptr;
}

SDL_Surface *surface_load_from_file(const char *file_path)
{
    int w, h, n;
    unsigned char *pixels = stbi_load(file_path, &w, &h, &n, STBI_rgb_alpha);
    if (!pixels) {
        fprintf(stderr, "Error: Failed to load surface. %s\n", stbi_failure_reason());
        exit(1);
    }

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    Uint32 rmask = 0x000000FF;
    Uint32 gmask = 0x0000FF00;
    Uint32 bmask = 0x00FF0000;
    Uint32 amask = 0xFF000000;
#else
    Uint32 rmask = 0xFF000000;
    Uint32 gmask = 0x00FF0000;
    Uint32 bmask = 0x0000FF00;
    Uint32 amask = 0x000000FF;
#endif

    int depth = 4 * 8;
    int pitch = 4 * w;
    SDL_Surface *surface = 
        scp(SDL_CreateRGBSurfaceFrom(
            pixels, w, h, depth, pitch, rmask, gmask, bmask, amask
        ));
    return surface;
}

SDL_Texture *texture_load_from_file(
    SDL_Renderer *renderer, const char *file_path, 
    bool set_trans_color, Uint32 color_key)
{
    if (!file_path) {
        fprintf(stderr, "Error: Passed null pointer for file_path.\n");
        exit(1);
    }

    SDL_Surface *surface = surface_load_from_file(file_path);        
    if (set_trans_color) { 
        scc(SDL_SetColorKey(surface, color_key, GET_COLOR(color_key)));
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        fprintf(stderr, "Error: Passed null pointer for file_path.\n");
        exit(1);
    }

    SDL_FreeSurface(surface);
    return texture;
}

typedef struct 
{
    float x;
    float y;
} Vec2f;

Vec2f vec2f(float x, float y)
{
    return (Vec2f) {
        .x = x,
        .y = y,
    };
}

Vec2f vec2fs(float x)
{
    return vec2f(x, x);
}

UNUSED void texture_render(SDL_Renderer *renderer, SDL_Texture *texture, Vec2f pos, float scale)
{
    Uint32 format; 
    int access, w, h;
    scc(SDL_QueryTexture(texture, &format, &access, &w, &h));
    SDL_Rect src = {
        .x = 0,
        .y = 0,
        .w = w,
        .h = h,
    };
    SDL_Rect dst = {
        .x = pos.x,
        .y = pos.y,
        .w = w * scale,
        .h = h * scale,
    };
    scc(SDL_RenderCopy(
        renderer, texture, &src, &dst
    ));
}

#define ASCII_DISPLAY_LOW 32
#define ASCII_DISPLAY_HIGH 126

typedef struct 
{
    SDL_Texture *spritesheet;
    SDL_Texture *spritesheet_under_cursor;
    SDL_Rect char_table[ASCII_DISPLAY_HIGH - ASCII_DISPLAY_LOW + 1];
    size_t char_width;
    size_t char_height;
} Font;

#define FONT_ROWS 7
#define FONT_COLS 18

// TODO: search if it is better to load the spritesheet again or copying it.
// My first impression is that copying is cheaper because it happens in memory.
// You will not load the file again. You can load the texture from the old surface directly.
// See: https://stackoverflow.com/questions/75873908/how-to-copy-a-texture-to-another-texture-without-pointing-to-the-same-texture
SDL_Texture* DuplicateTexture(SDL_Texture* tex, SDL_Renderer* renderer) {
    Uint32 format;
    int w, h;
    SDL_BlendMode blendmode;
    SDL_Texture* renderTarget;
    SDL_Texture* newTex;
    SDL_QueryTexture(tex, &format, NULL, &w, &h);
    SDL_GetTextureBlendMode(tex, &blendmode);
    renderTarget = SDL_GetRenderTarget(renderer);
    newTex = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(newTex, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer, newTex);
    SDL_RenderCopy(renderer, tex, NULL, NULL);
    SDL_SetTextureBlendMode(newTex, blendmode);
    SDL_SetRenderTarget(renderer, renderTarget);
    return newTex;
}

void font_load_from_file(SDL_Renderer *renderer, const char *file_path, Font *font)
{
    Uint32 trans_color = 0x000000FF;
    font->spritesheet = texture_load_from_file(renderer, file_path, true, trans_color);
    Uint32 format; 
    int access, w, h;
    scc(SDL_QueryTexture(font->spritesheet, &format, &access, &w, &h));
    font->char_width = w / FONT_COLS;
    font->char_height = h / FONT_ROWS;
    for (size_t i = ASCII_DISPLAY_LOW; i <= ASCII_DISPLAY_HIGH; ++i) {
        size_t index = i - ASCII_DISPLAY_LOW;
        size_t row = index / FONT_COLS;
        size_t col = index % FONT_COLS;
        font->char_table[index] = (SDL_Rect) {
            .x = col * font->char_width,
            .y = row * font->char_height,
            .w = font->char_width,
            .h = font->char_height,
        };
    }

    font->spritesheet_under_cursor = DuplicateTexture(font->spritesheet, renderer);
    scc(SDL_SetTextureColorMod(font->spritesheet_under_cursor , UNHEX(trans_color)));
    scc(SDL_SetTextureAlphaMod(font->spritesheet_under_cursor , GET_ALPHA(trans_color)));
}

void render_char(SDL_Renderer *renderer, char c, const Font *font, Vec2f pos, bool is_cursor_col, bool is_cursor_row, float scale)
{
    SDL_Rect dst = {
        .x = pos.x,
        .y = pos.y,
        .w = font->char_width * scale,
        .h = font->char_height * scale,
    };

    int index = c - ASCII_DISPLAY_LOW;
    scc(SDL_RenderCopy(
        renderer, (is_cursor_row && is_cursor_col)? font->spritesheet_under_cursor: font->spritesheet, &font->char_table[index], &dst
    ));
}

void render_text_sized(SDL_Renderer *renderer, const char *text, size_t size, const Font *font, Vec2f pos, size_t cursor, bool is_cursor_row, float scale)
{
    for (size_t i = 0; i < size; ++i) {
        render_char(renderer, text[i], font, pos, i==cursor, is_cursor_row, scale);
        pos.x += font->char_width * scale;
    }
}

typedef struct {
    char *buf;
    size_t size;
    size_t capacity;
} Line;

#define LINE_MIN_CAPACITY 32

void line_stretch(Line *line, size_t n)
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

void line_insert_text_at_cursor_sized(Line *line, const char *text, size_t text_size, size_t *cursor)
{
    line_stretch(line, text_size);
    
    memmove(line->buf + *cursor + text_size, line->buf + *cursor, line->size - *cursor);
    memcpy(line->buf + *cursor, text, text_size);
    *cursor += text_size;
    line->size += text_size;
}

void line_insert_text_at_cursor(Line *line, const char *text, size_t *cursor)
{
    size_t text_size = strlen(text);
    line_insert_text_at_cursor_sized(line, text, text_size, cursor);
}

void line_backspace(Line *line, size_t *cursor)
{
    if (cursor == 0) {
        return;
    }

    memmove(line->buf + *cursor - 1, line->buf + *cursor, line->size - *cursor);
    *cursor -= 1;
    line->size -= 1;
}

typedef struct {
    Line *lines;
    size_t size;
    size_t capacity;
    size_t cursor_row;
    size_t cursor_col;
} Editor;

#define EDITOR_MIN_CAPACITY 2

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

void fix_cursor_position(Editor *editor) 
{
    editor->cursor_col = 
        editor->cursor_col <= editor->lines[editor->cursor_row].size? 
        editor->cursor_col: editor->lines[editor->cursor_row].size;
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

void render_cursor(SDL_Renderer *renderer, const Editor *editor, const Font *font, float scale)
{
    size_t col = 
        editor->cursor_col <= editor->lines[editor->cursor_row].size? 
        editor->cursor_col: editor->lines[editor->cursor_row].size;
    SDL_Rect dst = {
        .x = col * font->char_width * scale,
        .y = editor->cursor_row * font->char_height * scale,
        .w = font->char_width * scale,
        .h = font->char_height * scale,
    };

    scc(SDL_SetRenderDrawColor(renderer, UNHEXA(0xFFFFFFFF)));
    scc(SDL_RenderFillRect(renderer, &dst));
}

// This is for debugging
UNUSED int main1(void)
{
    surface_load_from_file("charmap-oldschool_white.png");
    
    
    return 0;
}

UNUSED int main(void)
{
    scc(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window *window = 
        scp(SDL_CreateWindow(
            "tea", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
            WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE
        ));
    
    SDL_Renderer *renderer = 
        scp(SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

    Font font = {0};
    font_load_from_file(renderer, "charmap-oldschool_white.png", &font);

    Editor editor;
    editor_init(&editor);
    
    bool quit = false;
    while (!quit) {
        Uint32 start_time = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: {
                    quit = true;
                } break;

                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym) {
                        case SDLK_BACKSPACE: {
                            editor_backspace(&editor);
                        } break;
                        
                        case SDLK_LEFT: {
                            editor_left(&editor);
                        } break;

                        case SDLK_RIGHT: {
                            editor_right(&editor);
                        } break;

                        case SDLK_UP: {
                            editor_up(&editor);
                        } break;

                        case SDLK_DOWN: {
                            editor_down(&editor);
                        } break;

                        case SDLK_RETURN: {
                            editor_insert_new_line(&editor);
                        } break;
                    }
                } break;

                case SDL_TEXTINPUT: {
                    editor_insert_text(&editor, event.text.text);
                } break;
            }
        }

        scc(SDL_SetRenderDrawColor(renderer, UNHEXA(0x000000FF)));
        scc(SDL_RenderClear(renderer));
        render_cursor(renderer, &editor, &font, SCALE);
        Vec2f pos = vec2fs(0);
        for (size_t i = 0; i < editor.size; ++i) {
            render_text_sized(renderer, editor.lines[i].buf, editor.lines[i].size, &font, pos, editor.cursor_col, editor.cursor_row == i, SCALE);
            pos.y += font.char_height * SCALE;
        }
        SDL_RenderPresent(renderer);

        Uint32 duration = SDL_GetTicks() - start_time;
        if (duration < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - duration);
        }
    }

    SDL_Quit();
}