#include <SDL3/SDL_render.h>
#include <iostream>
#include <SDL3/SDL.h>

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL initialization failed: "
            << SDL_GetError()
            << '\n';


    return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

   if (!SDL_CreateWindowAndRenderer(
       "MyGUI",
       1280,
       720,
       SDL_WINDOW_RESIZABLE,
       &window,
       &renderer)) {

           std::cerr << "Window creation failed: "
               << SDL_GetError()
                << '\n';


           SDL_Quit();

           return 1;
       }

    bool running = true;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(
        renderer,
        30,
        30,
        30,
        255
        );

        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}
