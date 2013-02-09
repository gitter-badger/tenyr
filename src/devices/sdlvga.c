#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "SDL.h"
#include "SDL_image.h"

#include "common.h"
#include "device.h"
#include "sim.h"

#define SDLVGA_BASE (1ULL << 16)

#define RESOURCE_DIR    "rsrc"

#define ROWS 40
#define COLS 80
#define FONT_HEIGHT 12
#define FONT_WIDTH  8
#define CELL_OFFSET 0x10 /* where the VRAM *data* starts, after controls */

struct sdlvga_state {
    uint32_t control[CELL_OFFSET];
    uint32_t data[ROWS][COLS];
    SDL_Surface *screen, *sprite;
    enum { RUNNING, STOPPING, STOPPED } status;
};

static int put_character(struct sdlvga_state *state, unsigned row,
        unsigned col, unsigned char ch)
{
    SDL_Rect src = {
                .x = ch * FONT_WIDTH,
                .y = 0,
                .w = FONT_WIDTH,
                .h = FONT_HEIGHT
             },
             dst = {
                 .x = col * FONT_WIDTH,
                 .y = row * FONT_HEIGHT,
                 .w = FONT_WIDTH,
                 .h = FONT_HEIGHT
             };

    SDL_BlitSurface(state->sprite, &src, state->screen, &dst);
    SDL_UpdateRect(state->screen, dst.x, dst.y, dst.w, dst.h);

    return 0;
}

static int sdlvga_init(struct plugin_cookie *pcookie, void *cookie, int nargs, ...)
{
    struct sdlvga_state *state = *(void**)cookie;

    if (!state)
        state = *(void**)cookie = malloc(sizeof *state);

    *state = (struct sdlvga_state){ .status = RUNNING };

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        fatal(0, "Unable to init SDL: %s", SDL_GetError());

    if (!(state->screen = SDL_SetVideoMode(COLS * FONT_WIDTH,
                    ROWS * FONT_HEIGHT, 16, SDL_SWSURFACE)))
        fatal(0, "Unable to set VGA surface : %s", SDL_GetError());

    int flags = IMG_INIT_PNG;
    if (IMG_Init(flags) != flags)
        fatal(0, "sdlvga failed to initialise SDL_Image");

    const char filename[] = RESOURCE_DIR "/font.png";
    state->sprite = IMG_Load(filename);
    if (!state->sprite)
        fatal(0, "sdlvga failed to load font sprite `%s'", filename);

    SDL_Rect fullscreen = {
        .x = 0, .y = 0,
        .w = COLS * FONT_WIDTH,
        .h = ROWS * FONT_HEIGHT,
    };
    Uint32 black = SDL_MapRGB(state->screen->format,0,0,0);
    SDL_FillRect(state->screen, &fullscreen, black);
    SDL_UpdateRect(state->screen, fullscreen.x, fullscreen.y, fullscreen.w, fullscreen.h);

    return 0;
}

static int sdlvga_fini(void *cookie)
{
    struct sdlvga_state *state = cookie;

    SDL_FreeSurface(state->sprite);
    SDL_FreeSurface(state->screen);

    free(state);
    // Can't immediately call SDL_Quit() in case others are using it
    atexit(SDL_Quit);
    atexit(IMG_Quit);

    return 0;
}

static int sdlvga_op(void *cookie, int op, uint32_t addr, uint32_t *data)
{
    struct sdlvga_state *state = cookie;

    // TODO handle control settings
    if (addr - SDLVGA_BASE > CELL_OFFSET) {
        uint32_t base = SDLVGA_BASE + CELL_OFFSET;
        unsigned row = (addr - base) / COLS;
        unsigned col = (addr - base) % COLS;

        if (op == OP_WRITE) {
            state->data[row][col] = *data;
            put_character(state, row, col, *data & 0xff);
            //handle_update(state);
        } else if (op == OP_READ) {
            *data = state->data[row][col];
        }
    }

    return 0;
}

static int sdlvga_pump(void *cookie)
{
    struct sdlvga_state *state = cookie;

    SDL_Event event;
    if (state->status == RUNNING) {
        if (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    state->status = STOPPED;
                    debug(0, "sdlvga requested quit");
                    exit(0);
                default:
                    break;
            }
        }
    }

    return 0;
}

int sdlvga_add_device(struct device **device)
{
    **device = (struct device){
        .bounds = { SDLVGA_BASE, SDLVGA_BASE + CELL_OFFSET + (COLS * ROWS) - 1 },
        .ops = {
            .op = sdlvga_op,
            .init = sdlvga_init,
            .fini = sdlvga_fini,
            .cycle = sdlvga_pump,
        },
    };

    return 0;
}

