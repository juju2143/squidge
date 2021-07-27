#include <string.h>
#include <inttypes.h>
#include <quickjs/quickjs.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "modules.h"

JSClassID js_image_class_id;

static void js_image_finalizer(JSRuntime *rt, JSValue val)
{
    JSImageData *s = JS_GetOpaque(val, js_image_class_id);

    SDL_FreeSurface(s->surface);
    IMG_Quit();

    js_free_rt(rt, s);
}

static JSValue js_image_ctor(JSContext *ctx,
                             JSValueConst new_target,
                             int argc, JSValueConst *argv)
{
    JSImageData *s;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    if (argc < 1)
        s->path = "";
    else
    {
        s->path = JS_ToCString(ctx, argv[0]);
        if(s->path != NULL)
            s->surface = IMG_Load(s->path);
    }

    int imgFlags = IMG_INIT_PNG|IMG_INIT_JPG|IMG_INIT_WEBP;
    if(!(IMG_Init(imgFlags) & imgFlags))
        return JS_EXCEPTION;

    /* using new_target to get the prototype is necessary when the
       class is extended. */
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_image_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_image_load(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);

    if(argc > 0)
        s->path = JS_ToCString(ctx, argv[0]);
    if(s->path != NULL)
        s->surface = IMG_Load(s->path);
    if(s->surface != NULL)
        return JS_TRUE;

    return JS_FALSE;
}

static JSValue js_image_loaded(JSContext *ctx, JSValueConst this_val)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    return JS_NewBool(ctx, s->surface != NULL);
}

static JSValue js_image_path(JSContext *ctx, JSValueConst this_val)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    return JS_NewString(ctx, s->path);
}

static JSValue js_image_size(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    int size;
    if(magic)
        size = s->surface->h;
    else
        size = s->surface->w;
    return JS_NewUint32(ctx, size);
}

static JSValue js_image_get_pixels(JSContext *ctx, JSValueConst this_val)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    //SDL_LockSurface(s->surface);
    JSValue pixels = JS_NewArrayBuffer(ctx, s->surface->pixels, s->surface->h * s->surface->pitch, NULL, NULL, 0);
    //SDL_UnlockSurface(s->surface);
    return pixels;
}

static JSValue js_image_set_pixels(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    //SDL_LockSurface(s->surface);
    size_t size; // s->surface->h * s->surface->pitch
    uint8_t* buf = JS_GetArrayBuffer(ctx, &size, val);
    if(size > s->surface->h * s->surface->pitch) size = s->surface->h * s->surface->pitch;
    SDL_memcpy(s->surface->pixels, buf, size);
    //SDL_UnlockSurface(s->surface);
    return JS_UNDEFINED;
}

static JSClassDef js_image_class = {
    "Image",
    .finalizer = js_image_finalizer,
};

static const JSCFunctionListEntry js_image_proto_funcs[] = {
    JS_CFUNC_DEF("load", 1, js_image_load),
    JS_CGETSET_MAGIC_DEF("width", js_image_size, NULL, 0),
    JS_CGETSET_MAGIC_DEF("height", js_image_size, NULL, 1),
    JS_CGETSET_DEF("loaded", js_image_loaded, NULL),
    JS_CGETSET_DEF("path", js_image_path, NULL),
    JS_CGETSET_DEF("pixels", js_image_get_pixels, js_image_set_pixels),
};

static int js_image_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue image_proto, image_class;
    
    /* create the image class */
    JS_NewClassID(&js_image_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_image_class_id, &js_image_class);

    image_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, image_proto, js_image_proto_funcs, countof(js_image_proto_funcs));
    
    image_class = JS_NewCFunction2(ctx, js_image_ctor, "Image", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, image_class, image_proto);
    JS_SetClassProto(ctx, js_image_class_id, image_proto);
                      
    JS_SetModuleExport(ctx, m, "Image", image_class);
    return 0;
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_image
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_image_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Image");
    return m;
}