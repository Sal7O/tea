#include <SDL2/SDL.h>
#include <stdbool.h>
#include <errno.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "editor.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FPS 60
#define FRAME_TIME (1000 / (FPS))
#define SCALE 5.0f
#define HISTORY_TIME 20

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

Vec2f global_start;
Vec2f global_offset;

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
    for (size_t i = global_start.x; i < size && i < global_start.x + global_offset.x; ++i) {
        render_char(renderer, text[i], font, pos, i==cursor, is_cursor_row, scale);
        pos.x += font->char_width * scale;
    }
}

void render_cursor(SDL_Renderer *renderer, const Editor *editor, const Font *font, float scale)
{
    size_t col = 
    editor->cursor_col <= editor->lines[editor->cursor_row].size? 
    editor->cursor_col: editor->lines[editor->cursor_row].size;
    
    int x = (col - global_start.x) * font->char_width * scale;
    int y = (editor->cursor_row - global_start.y) * font->char_height * scale;
    SDL_Rect dst = {
        .x = x,
        .y = y,
        .w = font->char_width * scale,
        .h = font->char_height * scale,
    };
    
    scc(SDL_SetRenderDrawColor(renderer, UNHEXA(0xFFFFFFFF)));
    scc(SDL_RenderFillRect(renderer, &dst));
}

Vec2f window_get_dimensions(SDL_Window *window)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    return vec2f(w, h);
}

void window_update_start_position(const Editor *editor)
{
    if (editor->cursor_row - global_start.y + 1 > global_offset.y) {
        global_start.y = editor->cursor_row - global_offset.y + 1;
    }
    
    if (editor->cursor_col - global_start.x + 1 > global_offset.x) {
        global_start.x = editor->cursor_col - global_offset.x + 1;
    }
    
    if (editor->cursor_row < global_start.y) {
        global_start.y = editor->cursor_row;
    }
    
    if (editor->cursor_col < global_start.x) {
        global_start.x = editor->cursor_col;
    }
}

// When pressing down while the cursor is at the end of a long line and the next line is very short, window does not render that line
// and keeps rendering the end of the first line. not untill you do another action.
// my be it is because the actual cursor value is still not updated.
// TODO: use line size as the actual value of cursor to update the start.


typedef struct 
{
    Editor *editors;
    size_t size;
    size_t capacity;
    size_t index;
} History;

#define HISTORY_MIN_CAPACITY 2

void history_stretch(History *history, size_t n)
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

typedef enum 
{
    INSERT_NEWLINE_ACTION = -1,
    INSERT_TEXT_ACTION = -2,
    BACKSPACE_ACTION = -3,
    MOVE_LEFT_ACTION = 1,
    MOVE_RIGHT_ACTION = 2,
    MOVE_UP_ACTION = 3,
    MOVE_DOWN_ACTION = 4,
} Action;

typedef uint8_t uint8;

int main(int argc, char **argv)
{
    scc(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window *window = 
        scp(SDL_CreateWindow(
            "tea", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
            WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE
        ));
    
    SDL_Renderer *renderer = 
        scp(SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

    Font font = { 0 };
    font_load_from_file(renderer, "charmap-oldschool_white.png", &font);

    Editor editor;
    editor_init(&editor);
    if (argc > 1) {
        editor_load_file(&editor, argv[1]);
    }

    
    global_start = vec2fs(0);
    Vec2f win_dimensions = window_get_dimensions(window);
    global_offset = vec2f(
        ceilf(win_dimensions.x / (font.char_width * SCALE)),
        ceilf(win_dimensions.y / (font.char_height * SCALE))
    );


    History history = { 0 };
    Editor initial = { 0 };
    editor_deep_copy(&editor, &initial);
    Uint32 history_st = SDL_GetTicks();
    bool changed = false;

    bool quit = false;
    while (!quit) {
        Uint32 frame_st = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: {
                    quit = true;
                } break;

                case SDL_WINDOWEVENT: {
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED: {
                            Vec2f win_dimensions = window_get_dimensions(window);
                            global_offset = vec2f(
                                ceilf(win_dimensions.x / (font.char_width * SCALE)),
                                ceilf(win_dimensions.y / (font.char_height * SCALE))
                            );
                            global_start.x = editor.cursor_col - global_offset.x + 1;
                            global_start.y = editor.cursor_row - global_offset.y + 1;
                            if (global_start.x < 0) {
                                global_start.x = 0;
                            }
                            if (global_start.y < 0) {
                                global_start.y = 0;
                            }
                        } break;
                    }
                } break;

                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym) {
                        case SDLK_BACKSPACE: {
                            editor_backspace(&editor);
                            changed = true;
                            history_st = SDL_GetTicks();
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
                            changed = true;
                            history_st = SDL_GetTicks();
                        } break;

                        case SDLK_z: {
                            if (changed) {
                                Editor copy;
                                editor_deep_copy(&editor, &copy);
                                history_append(&history, &copy);
                                changed = false;
                            }

                            if ((event.key.keysym.mod & KMOD_CTRL) && (event.key.keysym.mod & KMOD_SHIFT)) {
                                history_redo(&history, &editor);
                            } else if (event.key.keysym.mod & KMOD_CTRL) {
                                history_undo(&history, &editor);
                            }
                        } break;

                        case SDLK_F1: {
                            if (argc > 1) {
                                editor_save_file(&editor, argv[1]);
                            }
                        } break;
                    }
                } break;

                case SDL_TEXTINPUT: {
                    editor_insert_text(&editor, event.text.text);
                    changed = true;
                    history_st = SDL_GetTicks();
                } break;
            }
        }

        window_update_start_position(&editor);

        scc(SDL_SetRenderDrawColor(renderer, UNHEXA(0x000000FF)));
        scc(SDL_RenderClear(renderer));
        render_cursor(renderer, &editor, &font, SCALE);
        Vec2f pos = vec2fs(0);
        for (size_t i = global_start.y; i < editor.size && i <= global_start.y + global_offset.y; ++i) {
            render_text_sized(
                renderer, editor.lines[i].buf, editor.lines[i].size, 
                &font, pos, editor.cursor_col, editor.cursor_row == i, SCALE
            );
            pos.y += font.char_height * SCALE;
        }
        SDL_RenderPresent(renderer);

        if (HISTORY_TIME < SDL_GetTicks() - history_st && changed) {
            Editor copy;
            editor_deep_copy(&editor, &copy);
            history_append(&history, &copy);
            changed = false;
        }

        Uint32 frame_duration = SDL_GetTicks() - frame_st;
        if (frame_duration < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - frame_duration);
        }
    }
    editor_free(&editor);
    SDL_Quit();
}