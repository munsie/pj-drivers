#include <SDL.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

// This is a fork of the Ingo Ruhnke sdl2-jstest, 
// simplified with reduced dependecies
// developed for testing sdl joysticks for the PJ project.
// -- pcbjunkie

/* functions *****************************************************************/

void simple_list_joysticks() {
    int num_joysticks = SDL_NumJoysticks();
    if (num_joysticks < 0)
        printf("No joysticks were found\n");
    else
        for(int i = 0; i < num_joysticks; i++)
            printf("%d:%s\n", i, SDL_JoystickNameForIndex(i));
}

int str2int(const char* str, int* val) {
    char* endptr;
    errno = 0;    /* To distinguish success/failure after call */

    *val = strtol(str, &endptr, 10);

    /* Check for various possible errors */
    if ((errno == ERANGE && (*val == LONG_MAX || *val == LONG_MIN))
            || (errno != 0 && *val == 0)) {
        return 0;
    }

    if (endptr == str) {
        return 0;
    }

    return 1;
}


void print_joystick_info(int joy_idx, SDL_Joystick* joy, SDL_GameController* gamepad) {
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joy);
    char guid_str[1024];
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

    printf("Joystick Name:     '%s'\n", SDL_JoystickName(joy));
    printf("Joystick GUID:     %s\n", guid_str);
    printf("Joystick Number:   %2d\n", joy_idx);
    printf("Number of Axes:    %2d\n", SDL_JoystickNumAxes(joy));
    printf("Number of Buttons: %2d\n", SDL_JoystickNumButtons(joy));
    printf("Number of Hats:    %2d\n", SDL_JoystickNumHats(joy));
    printf("Number of Balls:   %2d\n", SDL_JoystickNumBalls(joy));
    printf("GameController:\n");
    if (!gamepad) {
        printf("  not a gamepad\n");
    } else {
        printf("  Name:    '%s'\n", SDL_GameControllerName(gamepad));
        printf("  Mapping: '%s'\n", SDL_GameControllerMappingForGUID(guid));
    }
    printf("\n");
}


void list_joysticks() {
    int num_joysticks = SDL_NumJoysticks();
    if (num_joysticks == 0) {
        printf("No joysticks were found\n");
    } else {
        printf("Found %d joystick(s)\n\n", num_joysticks);
        for(int joy_idx = 0; joy_idx < num_joysticks; ++joy_idx) {
            SDL_Joystick* joy = SDL_JoystickOpen(joy_idx);
            if (!joy) {
                fprintf(stderr, "Unable to open joystick %d\n", joy_idx);
            } else {
                SDL_GameController* gamepad = SDL_GameControllerOpen(joy_idx);
                print_joystick_info(joy_idx, joy, gamepad);
                if (gamepad) {
                    SDL_GameControllerClose(gamepad);
                }
                SDL_JoystickClose(joy);
            }
        }
    }
}


void test_joystick(int joy_idx) {
    SDL_Joystick* joy = SDL_JoystickOpen(joy_idx);
    if (!joy) {
        fprintf(stderr, "Unable to open joystick %d\n", joy_idx);
    } else {

        int num_axes    = SDL_JoystickNumAxes(joy);
        int num_buttons = SDL_JoystickNumButtons(joy);
        int num_hats    = SDL_JoystickNumHats(joy);
        int num_balls   = SDL_JoystickNumBalls(joy);

        Sint16* axes    = calloc(num_axes,    sizeof(Sint16));
        Uint8*  buttons = calloc(num_buttons, sizeof(Uint8));
        Uint8*  hats    = calloc(num_hats,    sizeof(Uint8));
        Uint8*  balls   = calloc(num_balls,   2*sizeof(Sint16));

        int quit = 0;
        SDL_Event event;

        printf("Press Ctrl-c to exit\n\n");
        printf("Joystick Name:   '%s'\n", SDL_JoystickName(joy));
        printf("Joystick Number: %d\n", joy_idx);
        printf("\n");

        system("stty -echo"); // supress echo
        system("stty cbreak"); // go to RAW mode

        while(!quit) {
            SDL_Delay(10);

            int something_new = 1;
            while (SDL_PollEvent(&event)) {
                something_new = 1;
                switch(event.type) {
                    case SDL_JOYAXISMOTION:
                        assert(event.jaxis.axis < num_axes);
                        axes[event.jaxis.axis] = event.jaxis.value;
                        break;

                    case SDL_JOYBUTTONDOWN:
                    case SDL_JOYBUTTONUP:
                        assert(event.jbutton.button < num_buttons);
                        buttons[event.jbutton.button] = event.jbutton.state;
                        break;

                    case SDL_JOYHATMOTION:
                        assert(event.jhat.hat < num_hats);
                        hats[event.jhat.hat] = event.jhat.value;
                        break;

                    case SDL_JOYDEVICEADDED:
                        printf("Joystick %d added event detected\n", event.jdevice.which);
                        break;

                    case SDL_JOYDEVICEREMOVED:
                        printf("Joystick %d removed event detected\n", event.jdevice.which);
                        break;

                    case SDL_JOYBALLMOTION:
                        assert(event.jball.ball < num_balls);
                        balls[2*event.jball.ball + 0] = event.jball.xrel;
                        balls[2*event.jball.ball + 1] = event.jball.yrel;
                        break;

                    case SDL_QUIT:
                        quit = 1;
                        system ("stty echo"); // Make echo work
                        system("stty -cbreak");// go to COOKED mode
                        printf("\n");
                        printf("Recieved interrupt, exiting\n");
                        something_new = 0;
                        break;

                    default:
                        fprintf(stderr, "Error: Unhandled event type: %d\n", event.type);
                }
            }

            if (something_new) {

                printf("\rAxes %2d: ", num_axes);
                for(int i = 0; i < num_axes; ++i) {
                    printf("[%6d]", axes[i]);
                }
                printf(" - ");

                printf("Buttons %2d: ", num_buttons);
                for(int i = 0; i < num_buttons; ++i) {
                    printf("[%s]", buttons[i] ? "#":" ");
                }
                printf(" - ");

                printf("Hats %2d: ", num_hats);
                for(int i = 0; i < num_hats; ++i) {
                    printf("[%d]", hats[i]);
                }
                printf(" - ");

                printf("Balls %2d: ", num_balls);
                for(int i = 0; i < num_balls; ++i) {
                    printf("[%6d %6d]", balls[2*i+0], balls[2*i+1]);
                }
                printf("                       \r");
                fflush(stdout);

            }
        } // while

        free(balls);
        free(hats);
        free(buttons);
        free(axes);

    }
}


void test_gamecontroller_events(SDL_GameController* gamepad) {
    assert(gamepad);

    printf("Entering gamecontroller test loop, press Ctrl-c to exit\n");
    int quit = 0;
    SDL_Event event;

    while(!quit && SDL_WaitEvent(&event)) {
        switch(event.type) {
            case SDL_JOYAXISMOTION:
                //printf("SDL_JOYAXISMOTION: joystick: %d axis: %d value: %d\n",
                //       event.jaxis.which, event.jaxis.axis, event.jaxis.value);
                break;

            case SDL_JOYBUTTONDOWN:
                //printf("SDL_JOYBUTTONDOWN: joystick: %d button: %d state: %d\n",
                //       event.jbutton.which, event.jbutton.button, event.jbutton.state);
                break;

            case SDL_JOYBUTTONUP:
                //printf("SDL_JOYBUTTONUP: joystick: %d button: %d state: %d\n",
                //       event.jbutton.which, event.jbutton.button, event.jbutton.state);
                break;

            case SDL_JOYHATMOTION:
                //printf("SDL_JOYHATMOTION: joystick: %d hat: %d value: %d\n",
                //       event.jhat.which, event.jhat.hat, event.jhat.value);
                break;

            case SDL_JOYBALLMOTION:
                //printf("SDL_JOYBALLMOTION: joystick: %d ball: %d x: %d y: %d\n",
                //       event.jball.which, event.jball.ball, event.jball.xrel, event.jball.yrel);
                break;

            case SDL_JOYDEVICEADDED:
                //printf("SDL_JOYDEVICEADDED which:%d\n", event.jdevice.which);
                break;

            case SDL_JOYDEVICEREMOVED:
                //printf("SDL_JOYDEVICEREMOVED which:%d\n", event.jdevice.which);
                break;

            case SDL_CONTROLLERAXISMOTION:
                printf("SDL_CONTROLLERAXISMOTION controller: %d axis: %-12s value: %d\n",
                        event.caxis.which,
                        SDL_GameControllerGetStringForAxis(event.caxis.axis),
                        event.caxis.value);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
                printf("SDL_CONTROLLERBUTTONDOWN controller: %d button: %s state: %d\n",
                        event.cbutton.which,
                        SDL_GameControllerGetStringForButton(event.cbutton.button),
                        event.cbutton.state);
                break;

            case SDL_CONTROLLERBUTTONUP:
                printf("SDL_CONTROLLERBUTTONUP   controller: %d button: %s state: %d\n",
                        event.cbutton.which,
                        SDL_GameControllerGetStringForButton(event.cbutton.button),
                        event.cbutton.state);
                break;

            case SDL_CONTROLLERDEVICEADDED:
                printf("SDL_CONTROLLERDEVICEADDED which:%d\n", event.cdevice.which);
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                printf("SDL_CONTROLLERDEVICEREMOVED which:%d\n",  event.cdevice.which);
                break;

            case SDL_CONTROLLERDEVICEREMAPPED:
                printf("SDL_CONTROLLERDEVICEREMAPPED which:%d\n", event.cdevice.which);
                break;

            case SDL_QUIT:
                quit = 1;
                printf("Recieved interrupt, exiting\n");
                break;

            default:
                fprintf(stderr, "Error: Unhandled event type: %#x\n", event.type);
                break;
        }
    }
}


void test_gamecontroller_state(SDL_GameController* gamepad) {
    assert(gamepad);

    printf("Entering gamecontroller test loop, press Ctrl-c to exit\n");
    int quit = 0;
    SDL_Event event;

    while(!quit && SDL_WaitEvent(&event)) {
        switch(event.type) {
            case SDL_QUIT:
                quit = 1;
                printf("Recieved interrupt, exiting\n");
                break;
        }

        for(int btn = 0; btn < SDL_CONTROLLER_BUTTON_MAX; ++btn) {
            printf("%s:%d ",
                    SDL_GameControllerGetStringForButton(btn),
                    SDL_GameControllerGetButton(gamepad, btn));
        }

        for(int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
            printf("%s:%6d ",
                    SDL_GameControllerGetStringForAxis(axis),
                    SDL_GameControllerGetAxis(gamepad, axis));
        }

        printf("\n");
    }
}


void test_gamecontroller(int gamecontroller_idx) {
    SDL_GameController* gamepad = SDL_GameControllerOpen(gamecontroller_idx);
    if (!gamepad) {
        printf("error: not a gamecontroller\n");
    } else {
        //test_gamecontroller_events(gamepad);
        test_gamecontroller_state(gamepad);

        SDL_GameControllerClose(gamepad);
    }
}


void event_joystick(int joy_idx) {
    SDL_Joystick* joy = SDL_JoystickOpen(joy_idx);
    if (!joy) {
        fprintf(stderr, "Unable to open joystick %d\n", joy_idx);
    } else {
        print_joystick_info(joy_idx, joy, NULL);

        printf("Entering joystick test loop, press Ctrl-c to exit\n");
        int quit = 0;
        SDL_Event event;

        while(!quit && SDL_WaitEvent(&event)) {
            switch(event.type) {
                case SDL_JOYAXISMOTION:
                    printf("SDL_JOYAXISMOTION: joystick: %d axis: %d value: %d\n",
                            event.jaxis.which, event.jaxis.axis, event.jaxis.value);
                    break;

                case SDL_JOYBUTTONDOWN:
                    printf("SDL_JOYBUTTONDOWN: joystick: %d button: %d state: %d\n",
                            event.jbutton.which, event.jbutton.button, event.jbutton.state);
                    break;

                case SDL_JOYBUTTONUP:
                    printf("SDL_JOYBUTTONUP: joystick: %d button: %d state: %d\n",
                            event.jbutton.which, event.jbutton.button, event.jbutton.state);
                    break;

                case SDL_JOYHATMOTION:
                    printf("SDL_JOYHATMOTION: joystick: %d hat: %d value: %d\n",
                            event.jhat.which, event.jhat.hat, event.jhat.value);
                    break;

                case SDL_JOYBALLMOTION:
                    printf("SDL_JOYBALLMOTION: joystick: %d ball: %d x: %d y: %d\n",
                            event.jball.which, event.jball.ball, event.jball.xrel, event.jball.yrel);
                    break;

                case SDL_JOYDEVICEADDED:
                    printf("SDL_JOYDEVICEADDED which:%d\n", event.jdevice.which);
                    break;

                case SDL_JOYDEVICEREMOVED:
                    printf("SDL_JOYDEVICEREMOVED which:%d\n", event.jdevice.which);
                    break;

                case SDL_CONTROLLERBUTTONDOWN:
                    printf("SDL_CONTROLLERBUTTONDOWN\n");
                    break;

                case SDL_CONTROLLERBUTTONUP:
                    printf("SDL_CONTROLLERBUTTONUP\n");
                    break;

                case SDL_CONTROLLERDEVICEADDED:
                    printf("SDL_CONTROLLERDEVICEADDED which:%d\n", event.cdevice.which);
                    break;

                case SDL_CONTROLLERDEVICEREMOVED:
                    printf("SDL_CONTROLLERDEVICEREMOVED which:%d\n",  event.cdevice.which);
                    break;

                case SDL_CONTROLLERDEVICEREMAPPED:
                    printf("SDL_CONTROLLERDEVICEREMAPPED which:%d\n", event.cdevice.which);
                    break;

                case SDL_QUIT:
                    quit = 1;
                    printf("Recieved interrupt, exiting\n");
                    break;

                default:
                    fprintf(stderr, "Error: Unhandled event type: %d\n", event.type);
                    break;
            }
        }
        SDL_JoystickClose(joy);
    }
}

/* main() ********************************************************************/

void print_help(const char* prg) {
    printf("Usage: %s <option>\n", prg);
    printf("SDL joystick test utility\n");
    printf("Options:\n");
    printf("  -h : prints this help message\n");
    printf("  -l : list available joysticks\n");
    printf("  -ls : simple list of available joysticks\n");
    printf("  -t <joystick_num> : test joystick\n");
    printf("  -e <joystick_num> : get list of events from joystick\n");
    printf("  -g <gamepad_num> : test gamepad\n");
    printf("  -q <device_path> : query device in /dev/input\n");
}

int main(int argc, char** argv) {
    if (argc == 1) {
        print_help(argv[0]);
        exit(1);
    }

    // SDL2 will only report events when the window has focus, so set
    // this hint as we don't have a window
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    // FIXME: We don't need video, but without it SDL will fail to work in SDL_WaitEvent()
    if(SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        exit(1);
    } else {
        atexit(SDL_Quit);

        if (argc == 2 && (strcmp(argv[1], "-h") == 0)) print_help(argv[0]);
        else if (argc == 2 && (strcmp(argv[1], "-l") == 0)) list_joysticks();
        else if (argc == 2 && (strcmp(argv[1], "-ls") == 0)) simple_list_joysticks();
        else if (argc == 3 && (strcmp(argv[1], "-g") == 0)){ 
            int idx;
            if (!str2int(argv[2], &idx)) {
                fprintf(stderr, "Error: JOYSTICKNUM argument must be a number, but was '%s'\n", argv[2]);
                exit(1);
            } else {
                test_gamecontroller(idx);
            }
        }
        else if (argc == 3 && (strcmp(argv[1], "-t") == 0)){
            int joy_idx;
            if (!str2int(argv[2], &joy_idx)) {
                fprintf(stderr, "Error: JOYSTICKNUM argument must be a number, but was '%s'\n", argv[2]);
                exit(1);
            } else {
                test_joystick(joy_idx);
            }
        }
        else if (argc == 3 && (strcmp(argv[1], "-e") == 0)){
            int joy_idx;
            if (!str2int(argv[2], &joy_idx)) {
                fprintf(stderr, "Error: JOYSTICKNUM argument must be a number, but was '%s'\n", argv[2]);
                exit(1);
            }
            event_joystick(joy_idx);
        }
        else {
            fprintf(stderr, "%s: unknown arguments\n", argv[0]);
            fprintf(stderr, "Try '%s -h' for more informations\n", argv[0]);
        }
    }
}


