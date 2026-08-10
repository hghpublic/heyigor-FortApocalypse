// https://github.com/hghpublic/libsdl-org-SDL/blob/dev/examples/0/examples/input/03-gamepad-polling/gamepad-events.c


#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <print>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;

#define MOTION_EVENT_COOLDOWN 40


static const char* battery_state_string(SDL_PowerState state)
{
    switch (state)
    {
    case SDL_POWERSTATE_ERROR:
        return "ERROR";
    case SDL_POWERSTATE_UNKNOWN:
        return "UNKNOWN";
    case SDL_POWERSTATE_ON_BATTERY:
        return "ON BATTERY";
    case SDL_POWERSTATE_NO_BATTERY:
        return "NO BATTERY";
    case SDL_POWERSTATE_CHARGING:
        return "CHARGING";
    case SDL_POWERSTATE_CHARGED:
        return "CHARGED";
    default:
        break;
    }
    return "UNKNOWN";
}


/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("gamepad-events", 640, 480,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer))
    {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }


    std::print("Please plug in a gamepad.\n");

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS; /* end the program, reporting success to the OS.
                                 */
    }
    else if (event->type == SDL_EVENT_GAMEPAD_ADDED)
    {
        /* this event is sent for each hotplugged stick, but also each
         * already-connected gamepad during SDL_Init(). */
        const SDL_JoystickID which = event->gdevice.which;
        SDL_Gamepad* gamepad = SDL_OpenGamepad(which);
        if (!gamepad)
        {
            std::print("Gamepad #{} add, but not opened: {}\n",
                       (unsigned int)which, SDL_GetError());
        }
        else
        {
            char* mapping = SDL_GetGamepadMapping(gamepad);
            std::print("Gamepad #{} ('{}') added\n", (unsigned int)which,
                       SDL_GetGamepadName(gamepad));
            if (mapping)
            {
                std::print("Gamepad #{} mapping: {}\n", (unsigned int)which,
                           mapping);
                SDL_free(mapping);
            }
        }
    }
    else if (event->type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        const SDL_JoystickID which = event->gdevice.which;
        SDL_Gamepad* gamepad = SDL_GetGamepadFromID(which);
        if (gamepad)
        {
            SDL_CloseGamepad(gamepad); /* the gamepad was unplugged. */
        }
        std::print("Gamepad #{} removed\n", (unsigned int)which);
    }
    else if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
    {
        static Uint64 axis_motion_cooldown_time =
            0; /* these are spammy, only show every X milliseconds. */
        const Uint64 now = SDL_GetTicks();
        if (now >= axis_motion_cooldown_time)
        {
            const SDL_JoystickID which = event->gaxis.which;
            axis_motion_cooldown_time = now + MOTION_EVENT_COOLDOWN;
            std::print(
                "Gamepad #{} axis {} -> {}\n", (unsigned int)which,
                SDL_GetGamepadStringForAxis((SDL_GamepadAxis)event->gaxis.axis),
                (int)event->gaxis.value);
        }
    }
    else if ((event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) ||
             (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN))
    {
        const SDL_JoystickID which = event->gbutton.which;
        std::print("Gamepad #{} button {} -> {}\n", (unsigned int)which,
                   SDL_GetGamepadStringForButton(
                       (SDL_GamepadButton)event->gbutton.button),
                   event->gbutton.down ? "PRESSED" : "RELEASED");
    }
    else if (event->type == SDL_EVENT_JOYSTICK_BATTERY_UPDATED)
    {
        const SDL_JoystickID which = event->jbattery.which;
        if (SDL_IsGamepad(which))
        { /* this is only reported for joysticks, so make sure this joystick is
             _actually_ a gamepad. */
            std::print("Gamepad #{} battery -> {} - {}\n", (unsigned int)which,
                       battery_state_string(event->jbattery.state),
                       event->jbattery.percent);
        }
    }

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate)
{
    const Uint64 now = SDL_GetTicks();

    float prev_y = 0.0f;
    int winw = 640, winh = 480;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &winw, &winh);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    SDL_Quit();
    /* SDL will clean up the window/renderer for us. We let the gamepads leak.
     */
}
