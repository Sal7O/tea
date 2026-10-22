#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FPS 60
#define FRAME_TIME (1000 / (FPS))

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

int main(void)
{
    scc(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window *window = 
        scp(SDL_CreateWindow(
            "", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
            WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE
        ));
    
    SDL_Renderer *renderer = 
        scp(SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));
    
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
        SDL_RenderPresent(renderer);

        Uint32 duration = SDL_GetTicks() - start_time;
        if (duration < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - duration);
        }
    }

    SDL_Quit();
}