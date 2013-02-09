#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "SDL.h"
#include "SDL_image.h"

#include "common.h"
#include "device.h"
#include "sim.h"

#define SDLLED_BASE (0x100)

#define DIGIT_COUNT     4
#define DIGIT_WIDTH     38
#define DOT_WIDTH       6
#define DIGIT_HEIGHT    64
#define RESOURCE_DIR    "rsrc"

struct sdlled_state {
    uint32_t data[2];
    SDL_Window *window;
    SDL_Renderer *renderer;
    enum { RUNNING, STOPPING, STOPPED } status;
    SDL_Texture *digits[16];
    SDL_Texture *dots[2];
};

static int put_digit(struct sdlled_state *state, unsigned index, unsigned digit)
{
    SDL_Rect src = { .w = DIGIT_WIDTH, .h = DIGIT_HEIGHT },
             dst = {
                 .x = index * (DIGIT_WIDTH + DOT_WIDTH),
                 .w = DIGIT_WIDTH,
                 .h = DIGIT_HEIGHT
             };

    if (digit > 15)
        return -1;

    SDL_Texture *num = state->digits[digit];
    if (!num) {
        char filename[1024];
        snprintf(filename, sizeof filename, RESOURCE_DIR "/%d/%c.png",
                DIGIT_HEIGHT, "0123456789ABCDEF"[digit]);

        SDL_Surface *surf = IMG_Load(filename);
        num = state->digits[digit] = SDL_CreateTextureFromSurface(state->renderer, surf);
        SDL_FreeSurface(surf);
    }

    SDL_RenderCopy(state->renderer, num, &src, &dst);
    // TODO do periodic updates
    SDL_RenderPresent(state->renderer);

    return 0;
}

static int put_dot(struct sdlled_state *state, unsigned index, unsigned on)
{
    SDL_Rect src = { .w = DOT_WIDTH, .h = DIGIT_HEIGHT },
             dst = {
                 .x = (index + 1) * DIGIT_WIDTH + index * DOT_WIDTH,
                 .w = DOT_WIDTH,
                 .h = DIGIT_HEIGHT
             };

    SDL_Texture *dot = state->dots[on];
    if (!dot) {
        char filename[1024];
        snprintf(filename, sizeof filename, RESOURCE_DIR "/%d/dot_%s.png",
                DIGIT_HEIGHT, on ? "on" : "off");

        SDL_Surface *surf = IMG_Load(filename);
        dot = state->dots[on] = SDL_CreateTextureFromSurface(state->renderer, surf);
        SDL_FreeSurface(surf);
    }

    SDL_RenderCopy(state->renderer, dot, &src, &dst);
    // TODO do periodic updates
    SDL_RenderPresent(state->renderer);

    return 0;
}

static void decode_led(uint32_t data, int digits[4])
{
    for (unsigned i = 0; i < 4; i++)
        digits[i] = (data >> (i * 4)) & 0xf;
}

static void decode_dots(uint32_t data, int dots[4])
{
    for (unsigned i = 0; i < 4; i++)
        dots[i] = (data >> i) & 1;
}

static int sdlled_init(struct plugin_cookie *pcookie, void *cookie, int nargs, ...)
{
    struct sdlled_state *state = *(void**)cookie;

    if (!state)
        state = *(void**)cookie = malloc(sizeof *state);

    *state = (struct sdlled_state){ .status = RUNNING };

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        fatal(0, "Unable to init SDL: %s", SDL_GetError());

    state->window = SDL_CreateWindow("sdlled",
            0, 0, DIGIT_COUNT * (DIGIT_WIDTH + DOT_WIDTH), DIGIT_HEIGHT, SDL_WINDOW_SHOWN);

    if (!state->window)
        fatal(0, "Unable to set up LED surface : %s", SDL_GetError());

    state->renderer = SDL_CreateRenderer(state->window, -1, 0);

    int flags = IMG_INIT_PNG;
    if (IMG_Init(flags) != flags)
        fatal(0, "sdlled failed to initialise SDL_Image");

    return 0;
}

static int sdlled_fini(void *cookie)
{
    struct sdlled_state *state = cookie;

    for (unsigned i = 0; i < 16; i++)
        SDL_DestroyTexture(state->digits[i]);

    SDL_DestroyTexture(state->dots[0]);
    SDL_DestroyTexture(state->dots[1]);

    SDL_DestroyRenderer(state->renderer);
    SDL_DestroyWindow(state->window);
    free(state);
    // Can't immediately call SDL_Quit() in case others are using it
    atexit(SDL_Quit);
    atexit(IMG_Quit);

    return 0;
}

static int handle_update(struct sdlled_state *state)
{
    int digits[4];
    int dots[4];

    decode_led(state->data[0], digits);
    decode_dots(state->data[1], dots);

    debug(5, "sdlled : %x%c%x%c%x%c%x%c\n",
             digits[3], dots[3] ? '.' : ' ',
             digits[2], dots[2] ? '.' : ' ',
             digits[1], dots[1] ? '.' : ' ',
             digits[0], dots[0] ? '.' : ' ');

    for (unsigned i = 0; i < 4; i++) {
        put_digit(state, i, digits[3 - i]);
        put_dot(state, i, dots[3 - i]);
    }

    return 0;
}

static int sdlled_op(void *cookie, int op, uint32_t addr, uint32_t *data)
{
    struct sdlled_state *state = cookie;

    if (op == OP_WRITE) {
        state->data[addr - SDLLED_BASE] = *data;
        handle_update(state);
    } else if (op == OP_READ) {
        *data = state->data[addr - SDLLED_BASE];
    }

    return 0;
}

static int sdlled_pump(void *cookie)
{
    struct sdlled_state *state = cookie;

    SDL_Event event;
    if (state->status == RUNNING) {
        if (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    state->status = STOPPED;
                    debug(0, "sdlled requested quit");
                    exit(0);
                default:
                    break;
            }
        }
    }

    return 0;
}

int sdlled_add_device(struct device **device)
{
    **device = (struct device){
        .bounds = { SDLLED_BASE, SDLLED_BASE + 1 },
        .ops = {
            .op = sdlled_op,
            .init = sdlled_init,
            .fini = sdlled_fini,
            .cycle = sdlled_pump,
        },
    };

    return 0;
}

