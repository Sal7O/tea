#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FPS 60
#define FRAME_TIME (1000 / (FPS))
#define SCALE 5.0f

#define UNUSED __attribute__((unused))

#if SDL_BYTEORDER == SDL_LIL_ENDIAN

#define GET_COLOR(color)    ((((color) << (8 * 3)) & 0xff) << (8 * 0)) + \
                            ((((color) << (8 * 2)) & 0xff) << (8 * 1)) + \
                            ((((color) << (8 * 1)) & 0xff) << (8 * 2)) + \
                            ((((color) << (8 * 0)) & 0xff) << (8 * 3))

#else

#define GET_COLOR(color)    (color)

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
    stbi_image_free(pixels);
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
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        fprintf(stderr, "Error: Passed null pointer for file_path.\n");
        exit(1);
    }
    
    if (set_trans_color) { 
        scc(SDL_SetColorKey(surface, color_key, GET_COLOR(color_key)));
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
    SDL_Rect char_table[ASCII_DISPLAY_HIGH - ASCII_DISPLAY_LOW + 1];
    size_t char_width;
    size_t char_height;
} Font;

#define FONT_ROWS 7
#define FONT_COLS 18

void font_load_from_file(SDL_Renderer *renderer, const char *file_path, Font *font)
{
    font->spritesheet = texture_load_from_file(renderer, file_path, true, 0x000000FF);
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
}

void render_char(SDL_Renderer *renderer, const char c, Font *font, Vec2f pos, float scale)
{
    SDL_Rect dst = {
        .x = pos.x,
        .y = pos.y,
        .w = font->char_width * scale,
        .h = font->char_height * scale,
    };

    int index = c - ASCII_DISPLAY_LOW;
    scc(SDL_RenderCopy(
        renderer, font->spritesheet, &font->char_table[index], &dst
    ));
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
            "", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
            WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE
        ));
    
    SDL_Renderer *renderer = 
        scp(SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

    Font font = {0};
    font_load_from_file(renderer, "charmap-oldschool_white.png", &font);
    
    bool quit = false;
    while (!quit) {
        Uint32 start_time = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: {
                    quit = true;
                } break;
            }
        }

        scc(SDL_RenderClear(renderer));
        render_char(renderer, 'a', &font, vec2fs(0), SCALE);
        SDL_RenderPresent(renderer);

        Uint32 duration = SDL_GetTicks() - start_time;
        if (duration < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - duration);
        }
    }

    SDL_Quit();
}