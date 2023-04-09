#ifdef OS_WINDOWS
#include <windows.h>
#include <stdio.h>
#endif

#include <GL/gl.h>
#include <SDL2/SDL.h>

typedef unsigned long long u64;
typedef long long i64;

#include <time.h>

#ifdef OS_LINUX
#include <unistd.h>
// Get a timestamp in micro seconds
static u64 os_utime(void) {
  struct timespec t = {};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000 * 1000 + t.tv_nsec / 1000;
}

static void os_usleep(u64 time) { usleep(time / 1000); }
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

static void os_usleep(u64 time) { Sleep(time / 1000); }
#endif



#undef main
int main(int argc, char *argv[]) {
  // load sdl and create a window
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Window *win = SDL_CreateWindow("nolag", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  SDL_GL_CreateContext(win);

  // enable vsync
  SDL_GL_SetSwapInterval(1);

  int mouse_x = 0;
  int mouse_y = 0;

  int opt_clear = 0;
  int opt_vsync = 1;
  int opt_sleep = 0;
  int opt_full  = 0;
  
  u64 dt_draw   = 0;
  u64 dt_swap   = 0;
  u64 dt_sleep  = 0;
  u64 dt_real   = 0;
  u64 time = 0;
  for (;;) {
    u64 t0 = os_utime();

    SDL_DisplayMode mode = {};
    SDL_GetWindowDisplayMode(win, &mode);
    u64 dt_target = 1000000 / mode.refresh_rate;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) return 0;
      if (ev.type == SDL_MOUSEMOTION) {
        mouse_x = ev.motion.x;
        mouse_y = ev.motion.y;
      }

      if(ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        if(ev.key.keysym.sym == SDLK_1) opt_vsync = !opt_vsync;
        if(ev.key.keysym.sym == SDLK_2) opt_clear = !opt_clear;
        if(ev.key.keysym.sym == SDLK_3) opt_sleep = !opt_sleep;
        if(ev.key.keysym.sym == SDLK_4) opt_full  = !opt_full;
        SDL_GL_SetSwapInterval(opt_vsync);
        SDL_SetWindowFullscreen(win, opt_full * SDL_WINDOW_FULLSCREEN_DESKTOP);
      }
    }

    // setup opengl size
    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSize(win, &window_w, &window_h);
    glViewport(0, 0, window_w, window_h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_w, window_h, 0, 0, 1);

    // clear screen
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    float s = 100;
    glBegin(GL_TRIANGLES);
    glColor3f(0, 0, 1);
    glVertex2f(mouse_x + 0, mouse_y + 0);
    glVertex2f(mouse_x + s, mouse_y + 0);
    glVertex2f(mouse_x + s, mouse_y - s);
    glEnd();
    u64 t1 = os_utime();
    SDL_GL_SwapWindow(win);
    if(opt_clear) glClear(GL_COLOR_BUFFER_BIT);
    u64 t2 = os_utime();
    volatile int i = 0;
    while(opt_sleep && (os_utime() - t2) + dt_draw + 5000 < dt_target) { os_usleep(0); }
    while(opt_sleep && (os_utime() - t2) + dt_draw +  400 < dt_target) { i+= 1; }
    u64 t3 = os_utime();

    // calcluate the time values
    float a = 0.05;
    dt_draw  = (1-a)*dt_draw  + a*(t1 - t0);
    dt_swap  = (1-a)*dt_swap  + a*(t2 - t1);
    dt_sleep = (1-a)*dt_sleep + a*(t3 - t2);
    dt_real  = (1-a)*dt_real  + a*(t3 - t0);

    time += t3-t0;
    if(time > 200000) {
      time -= 200000;
      printf("----------------------\n");
      printf("vsync: %d   (press 1)\n", opt_vsync);
      printf("clear: %d   (press 2)\n", opt_clear);
      printf("sleep: %d   (press 3)\n", opt_sleep);
      printf("\n");
      printf("dt_target: %6llu us\n", dt_target);
      printf("dt_real:   %6llu us\n", dt_real);
      printf("overshoot: %6lli us\n", (i64) dt_real - (i64) dt_target);
      printf("input lag: %6llu us\n", dt_swap + dt_draw);
      printf("\n");
      printf("dt_swap:   %6llu us\n", dt_swap);
      printf("dt_draw:   %6llu us\n", dt_draw);
      printf("dt_sleep:  %6llu us\n", dt_sleep);
    }
  }
  return 0;
}
