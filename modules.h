#ifndef MODULES_H
#define MODULES_H

#include <quickjs/quickjs.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#ifdef __cplusplus
extern "C" {
#endif

#define countof(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    SDL_Window* window;
    SDL_Surface* surface;
    int width;
    int height;
    int x;
    int y;
    char* title;
    Uint32 fullscreen;
    float opacity;
    SDL_bool resizable;
    SDL_bool borders;
} JSGfxData;

typedef struct {
    SDL_Surface* surface;
    char* path;
} JSImageData;

extern JSClassID js_gfx_class_id;
extern JSClassID js_image_class_id;

JSModuleDef *js_init_module_gfx(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_image(JSContext *ctx, const char *module_name);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* MODULES_H */