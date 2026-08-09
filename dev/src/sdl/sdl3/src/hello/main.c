// main.c
// Example program:
// Using SDL3 to create an application window
// https://wiki.libsdl.org/SDL3/SDL_CreateWindow

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO); // Initialize SDL3

    // Create an application window with the following settings:
    SDL_Window* window = SDL_CreateWindow("An SDL3 window", // window title
                              640,              // width, in pixels
                              480,              // height, in pixels
                              SDL_WINDOW_OPENGL // flags - see below
    );

    // Check that the window was successfully created
    if (window == NULL)
    {
        // In the case that the window could not be made...
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n",
                     SDL_GetError());
        return 1;
    }

    bool done = false;
    while (!done)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                done = true;
            }
        }

        // Do game logic, present a frame, etc.
    }

    // Close and destroy the window
    SDL_DestroyWindow(window);

    // Clean up
    SDL_Quit();
    return 0;
}
