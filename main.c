#ifdef OS_WINDOWS
#include <stdio.h>
#include <windows.h>
#endif

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <time.h>

// dont use SDL_main
#undef main

// u32 <=> 32 bit unsigned integer
// i32 <=> 32 bit   signed integer
typedef unsigned long long u64;
typedef signed   long long i64;
typedef unsigned       int u32;
typedef signed         int i32;

#ifdef OS_LINUX
#include <unistd.h>
// Get a timestamp in micro seconds
static u64 os_utime(void) {
    struct timespec t = {};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000 * 1000 + t.tv_nsec / 1000;
}

static void os_usleep(u64 time) {
    usleep(time / 1000);
}
#endif

#ifdef OS_WINDOWS
// Get a timestamp in micro seconds
static u64 os_utime(void) {
    LARGE_INTEGER big_freq;
    QueryPerformanceFrequency(&big_freq);
    LARGE_INTEGER big_count;
    QueryPerformanceCounter(&big_count);
    i64 freq = big_freq.QuadPart;
    i64 count = big_count.QuadPart;
    i64 time = count / (freq / 1000 / 1000);
    return (u64)time;
}

static void os_usleep(u64 time) {
    Sleep(time / 1000);
}
#endif

// --- OBSERVATIONS ---
// An x11 window in plasma wayland creates an additional frame of input lag
// because the SDL_GL_SwapBuffers does not actually wait for the next vblank
// this is done by the next first call to glClear. But if this glClear
// happens after polling for input in the next frame, then this adds a frame of
// lag this can be solved by adding an glClear after the buffer swap. This does
// not happen with a wayland window. run 'SDL_VIDEODRIVER=wayland ./nolag'
//
// On plasma wayland AMD RX580
// allowing tearing and choosing best latency settings helps a bit
// enabling adaptive sync on kde feels better, but can't really measure it yet
// x11                 = ~1 frame  of input lag
// x11,vsync           =  4 frames of input lag
// x11,vsync,clear     =  3 frames of input lag
//
// wayland             =  0 frames of input lag
// wayland,sleep       =  0 frames of input lag
// wayland,vsync       =  2 frames of input lag
// wayland,vsync,clear =  2 frames of input lag
//
// so: vsync=0 + sleep + wayland is best
//

int main(int argc, char *argv[]) {
    // load sdl and create a window
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window *win = SDL_CreateWindow(
        "nolag",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(0);

    // options
    int opt_early_clear = 0;
    int opt_vsync       = 0; // toggle vsync
    int opt_sleep       = 0; // toggle manual sleep
    int opt_full        = 0; // toggle fullscreen
    int opt_predict     = 0; // number of frames to predict forward
    int opt_tear        = 0; // flkker screen to find tearing

    int mouse_x = 0;
    int mouse_y = 0;
    int mouse_old_x = 0;
    int mouse_old_y = 0;
    u64 time_to_show_information = 0;

    u32 frame_counter = 0;

    u64 time = os_utime();
    u64 dt_target = 1e6 / 240;
    for (;;) {
        // -------- INPUT --------
        u64 t0_input = os_utime();

        SDL_DisplayMode mode = {};
        SDL_GetWindowDisplayMode(win, &mode);
        u64 dt_monitor = 1000000 / mode.refresh_rate;

        int window_w = 0;
        int window_h = 0;
        SDL_GetWindowSize(win, &window_w, &window_h);

        // store old mouse position
        // used to calculate the mouse velocity
        mouse_old_x = mouse_x;
        mouse_old_y = mouse_y;
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                return 0;
            if (ev.type == SDL_MOUSEMOTION) {
                mouse_x = ev.motion.x;
                mouse_y = ev.motion.y;
            }

            if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.sym == SDLK_1) opt_vsync = !opt_vsync;
                if (ev.key.keysym.sym == SDLK_2) opt_full = !opt_full;
                if (ev.key.keysym.sym == SDLK_3) opt_predict -= 1;
                if (ev.key.keysym.sym == SDLK_4) opt_predict += 1;
                if (ev.key.keysym.sym == SDLK_5) opt_early_clear = !opt_early_clear;
                if (ev.key.keysym.sym == SDLK_6) opt_sleep = !opt_sleep;
                if (ev.key.keysym.sym == SDLK_7) opt_tear = !opt_tear;

                // apply new settings
                SDL_GL_SetSwapInterval(opt_vsync);
                SDL_SetWindowFullscreen(win, opt_full * SDL_WINDOW_FULLSCREEN_DESKTOP);
                // show info immediately
                time_to_show_information = 0;
            }
        }

        // -------- UPDATE AND RENDER --------
        // compute the next frame and issue opengl calls
        u64 t1_compute = os_utime();

        // compute the mouse speed (exponentially smoothed)
        // note that these two are the same:
        // x  = (1-a)*x + a*y;
        // x += a*(y-x)
        float mouse_dx = mouse_x - mouse_old_x;
        float mouse_dy = mouse_y - mouse_old_y;

        // start drawing
        glViewport(0, 0, window_w, window_h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, window_w, window_h, 0, 0, 1);
        if (opt_tear && frame_counter++ % 2 == 0) {
            glClearColor(0, 0, 0.3, 1);
        } else {
            glClearColor(0, 0, 0, 1);
        }
        glClear(GL_COLOR_BUFFER_BIT);

        // draw guides to help find the input lag in frames
        {
            float cross_size = 100;
            float x = mouse_x;
            float y = mouse_y;
            glBegin(GL_LINES);
            glColor3f(0, 0, 1);
            if (0) {
                glVertex2f(x + cross_size * .2, y + cross_size * .2);
                glVertex2f(x - cross_size * .2, y - cross_size * .2);
                glVertex2f(x - cross_size * .2, y + cross_size * .2);
                glVertex2f(x + cross_size * .2, y - cross_size * .2);
            }
            x += mouse_dx * opt_predict;
            y += mouse_dy * opt_predict;
            glColor3f(1, 0, 0);
            glVertex2f(x, y - cross_size);
            glVertex2f(x, y + cross_size);
            glVertex2f(x - cross_size, y);
            glVertex2f(x + cross_size, y);
            glEnd();
        }

        // -------- SWAP BUFFERS --------
        u64 t2_swap = os_utime();
        SDL_GL_SwapWindow(win);
        // When using the X11 window in plasma Wayland the SDL_GL_SwapWindow does
        // not wait for the next vblak however the next call to opengl does
        // (glClear) But this adds an additional frame of input lag if we read user
        // input before the clear
        if (opt_early_clear)
            glClear(GL_COLOR_BUFFER_BIT);

        // -------- SLEEP --------
        u64 t3_sleep = os_utime();
        time += dt_target;
        u32 count = 0;
        while (os_utime() < time) {
            os_usleep(0);
            count++;
        }
        u64 t4_frame_end = os_utime();

        u64 dt0_input = t1_compute - t0_input;
        u64 dt1_compute = t2_swap - t1_compute;
        u64 dt2_swap = t3_sleep - t2_swap;
        u64 dt3_sleep = t4_frame_end - t3_sleep;

        if (t0_input > time_to_show_information) {
            time_to_show_information = t0_input + 1000 * 1000;
            printf("----------------------------\n");
            printf("options:\n");
            printf("  vsync          = %d (press 1)\n", opt_vsync);
            printf("  fullscreen     = %d (press 2)\n", opt_full);
            printf("  predict frames = %d (press 3 and 4)\n", opt_predict);
            printf("  early clear    = %d (press 5)\n", opt_early_clear);
            printf("  extra sleep    = %d (press 6)\n", opt_sleep);
            printf("  tearing        = %d (press 7)\n", opt_tear);
            printf("\n");
            printf("measured:\n");
            printf("  input   = %6llu us\n", dt0_input);
            printf("  compute = %6llu us\n", dt1_compute);
            printf("  swap    = %6llu us\n", dt2_swap);
            printf("  sleep   = %6llu us\n", dt3_sleep);
            printf("  count   = %6u us\n", count);
            printf("\n");
        }
    }
    return 0;
}
