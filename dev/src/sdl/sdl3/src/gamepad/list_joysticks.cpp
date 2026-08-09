// main.cpp

#include <SDL3/SDL.h>

#include <print>
#include <ranges>
// #include <generator>

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::print("SDL_Init Error: {}", SDL_GetError());
        return -1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Logitech F310 SDL3 Example", 640, 480, 0);
    if (!window)
    {
        std::print("Window Error: {}", SDL_GetError());
        SDL_Quit();
        return -1;
    }


    SDL_Gamepad* gamepad = nullptr;
    bool running = true;
    SDL_Event event;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            }
        }

        std::print("Polling for joysticks...\n");
        int count = 0;
        SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);

        if (joysticks)
        {
            for (auto i : std::views::iota(0, count))
            {
                SDL_JoystickID id = joysticks[i];
                const char* name = SDL_GetJoystickNameForID(id);

                std::print("Joystick {}: ID = {}, Name = {}\n", i, id, name);
            }
            SDL_free(joysticks);
        }

        SDL_Delay(1000);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
