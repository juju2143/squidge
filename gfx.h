#ifndef GFX_H
#define GFX_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSModuleDef *js_init_module_gfx(JSContext *ctx, const char *module_name);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* GFX_H */