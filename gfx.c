#include <string.h>
#include <quickjs/quickjs.h>
#include <SDL2/SDL.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    SDL_Window* window;
    SDL_Surface* surface;
    int width;
    int height;
    char* title;
} JSGfxData;

static JSClassID js_gfx_class_id;

static void js_gfx_finalizer(JSRuntime *rt, JSValue val)
{
    JSGfxData *s = JS_GetOpaque(val, js_gfx_class_id);

    if(s->window != NULL)
    {
        SDL_DestroyWindow(s->window);
        s->window = NULL;
        SDL_Quit();
    }

    js_free_rt(rt, s);
}

static JSValue js_gfx_ctor(JSContext *ctx,
                             JSValueConst new_target,
                             int argc, JSValueConst *argv)
{
    JSGfxData *s;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    if (argc < 1 || JS_ToInt32(ctx, &s->width, argv[0]))
        s->width = 800;
    if (argc < 2 || JS_ToInt32(ctx, &s->height, argv[1]))
        s->height = 600;
    if (argc < 3)
        s->title = "Squidge";
    else
        s->title = JS_ToCString(ctx, argv[2]);
    /* using new_target to get the prototype is necessary when the
       class is extended. */
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_gfx_class_id);
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

static JSValue js_gfx_get_size(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if (!s)
        return JS_EXCEPTION;
    if(s->window != NULL)
        SDL_GetWindowSize(s->window, &s->width, &s->height);
    if (magic == 0)
        return JS_NewInt32(ctx, s->width);
    else
        return JS_NewInt32(ctx, s->height);
}

static JSValue js_gfx_set_size(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    int v;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &v, val))
        return JS_EXCEPTION;
    if (magic == 0)
        s->width = v;
    else
        s->height = v;
    if(s->window != NULL)
        SDL_SetWindowSize(s->window, s->width, s->height);
    return JS_UNDEFINED;
}

static JSValue js_gfx_get_title(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if (!s)
        return JS_EXCEPTION;

    if(s->window != NULL)
        s->title = SDL_GetWindowTitle(s->window);

    return JS_NewString(ctx, s->title);
}

static JSValue js_gfx_set_title(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if (!s)
        return JS_EXCEPTION;

    s->title = JS_ToCString(ctx, val);
    if(s->window != NULL)
        SDL_SetWindowTitle(s->window, s->title);

    return JS_UNDEFINED;
}

static JSValue js_gfx_delay(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    int s;
    if (JS_ToInt32(ctx, &s, argv[0]))
        return JS_UNDEFINED;
    SDL_Delay(s);
    return JS_UNDEFINED;
}

static JSValue js_gfx_quit(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if(s->window == NULL) return JS_EXCEPTION;
    SDL_DestroyWindow(s->window);
    s->window = NULL;
    SDL_Quit();

    return JS_UNDEFINED;
}

static JSValue js_gfx_update(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if(s->window == NULL) return JS_EXCEPTION;
    SDL_UpdateWindowSurface(s->window);

    return JS_UNDEFINED;
}

typedef struct eventlist {
    struct eventlist *next;
    int type;
    JSValue func;
    JSValue this;
} event_t;

static event_t *events = NULL;

JSValue createEventObject(JSContext *ctx, SDL_Event e)
{
    JSValue ev = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ev, "type", JS_NewUint32(ctx, e.type));
    switch(e.type)
    {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.key.timestamp));
            JS_SetPropertyStr(ctx, ev, "windowID", JS_NewUint32(ctx, e.key.windowID));
            JS_SetPropertyStr(ctx, ev, "state", JS_NewBool(ctx, e.key.state != 0));
            JS_SetPropertyStr(ctx, ev, "repeat", JS_NewBool(ctx, e.key.repeat != 0));
            JS_SetPropertyStr(ctx, ev, "scancode", JS_NewUint32(ctx, e.key.keysym.scancode));
            JS_SetPropertyStr(ctx, ev, "keycode", JS_NewUint32(ctx, e.key.keysym.sym));
            JS_SetPropertyStr(ctx, ev, "mod", JS_NewUint32(ctx, e.key.keysym.mod));
            break;
        case SDL_MOUSEMOTION:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.motion.timestamp));
            JS_SetPropertyStr(ctx, ev, "windowID", JS_NewUint32(ctx, e.motion.windowID));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewUint32(ctx, e.motion.which));
            JS_SetPropertyStr(ctx, ev, "state", JS_NewUint32(ctx, e.motion.state));
            JS_SetPropertyStr(ctx, ev, "x", JS_NewInt32(ctx, e.motion.x));
            JS_SetPropertyStr(ctx, ev, "y", JS_NewInt32(ctx, e.motion.y));
            JS_SetPropertyStr(ctx, ev, "xrel", JS_NewInt32(ctx, e.motion.xrel));
            JS_SetPropertyStr(ctx, ev, "yrel", JS_NewInt32(ctx, e.motion.yrel));
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.button.timestamp));
            JS_SetPropertyStr(ctx, ev, "windowID", JS_NewUint32(ctx, e.button.windowID));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewUint32(ctx, e.button.which));
            JS_SetPropertyStr(ctx, ev, "button", JS_NewUint32(ctx, e.button.button));
            JS_SetPropertyStr(ctx, ev, "state", JS_NewBool(ctx, e.button.state != 0));
            JS_SetPropertyStr(ctx, ev, "clicks", JS_NewUint32(ctx, e.button.clicks));
            JS_SetPropertyStr(ctx, ev, "x", JS_NewInt32(ctx, e.button.x));
            JS_SetPropertyStr(ctx, ev, "y", JS_NewInt32(ctx, e.button.y));
            break;
        case SDL_MOUSEWHEEL:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.wheel.timestamp));
            JS_SetPropertyStr(ctx, ev, "windowID", JS_NewUint32(ctx, e.wheel.windowID));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewUint32(ctx, e.wheel.which));
            JS_SetPropertyStr(ctx, ev, "x", JS_NewInt32(ctx, e.wheel.x));
            JS_SetPropertyStr(ctx, ev, "y", JS_NewInt32(ctx, e.wheel.y));
            JS_SetPropertyStr(ctx, ev, "direction", JS_NewBool(ctx, e.wheel.direction != 0));
            break;
        case SDL_QUIT:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.quit.timestamp));
            break;
        default:
            break;
    }
    return ev;
}

void callEvent(JSContext *ctx, SDL_Event e)
{
    event_t *ev;
    for(ev = events; ev != NULL; ev = ev->next)
    {
        if(e.type == ev->type)
        {
            int fargc = 1;
            JSValue* fargv = (JSValue*)malloc(sizeof(JSValue) * fargc);
            fargv[0] = createEventObject(ctx, e);
            JS_Call(ctx, ev->func, ev->this, fargc, fargv);
            for(int i = 0; i<fargc; i++) JS_FreeValue(ctx, fargv[i]);
        }
    }
}

static JSValue js_gfx_pollevent(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic)
{
    SDL_Event e;
    if(magic == 0)
    {
        while(SDL_PollEvent(&e) != 0) callEvent(ctx, e);
    }
    else if(argc > 0)
    {
        int time;
        if(JS_ToInt32(ctx, &time, argv[0]))
        {
            if(SDL_WaitEvent(&e) != 0) callEvent(ctx, e);
        }
        else
        {
            if(SDL_WaitEventTimeout(&e, time) != 0) callEvent(ctx, e);
        }
    }
    else
    {
        if(SDL_WaitEvent(&e) != 0) callEvent(ctx, e);
    }
    return JS_UNDEFINED;
}

static JSValue js_gfx_events(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic)
{
    if(argc == 0) return JS_EXCEPTION;

    const char* name = JS_ToCString(ctx, argv[0]);

    int event = 0;
    if(strcmp(name, "keydown") == 0) event = SDL_KEYDOWN;
    if(strcmp(name, "keyup") == 0) event = SDL_KEYUP;
    if(strcmp(name, "mousemove") == 0) event = SDL_MOUSEMOTION;
    if(strcmp(name, "mousedown") == 0) event = SDL_MOUSEBUTTONDOWN;
    if(strcmp(name, "mouseup") == 0) event = SDL_MOUSEBUTTONUP;
    if(strcmp(name, "mousewheel") == 0) event = SDL_MOUSEWHEEL;
    if(strcmp(name, "quit") == 0) event = SDL_QUIT;

    if(magic == 1)
    {
        if(argc > 1)
        {
            if(!JS_IsObject(argv[1])) return JS_EXCEPTION;
            event_t *ev = events;
            if(ev != NULL)
            {
                while(ev->next != NULL) ev = ev->next;
                ev->next = (event_t*)malloc(sizeof(event_t));
                //if(ev->next == NULL) return JS_EXCEPTION;
                ev->next->type = event;
                ev->next->func = argv[1];
                ev->next->this = argv[1];
                ev->next->next = NULL;
            }
            else
            {
                events = (event_t*)malloc(sizeof(event_t));
                //if(ev->next == NULL) return JS_EXCEPTION;
                events->type = event;
                events->func = argv[1];
                events->this = argv[1];
                events->next = NULL;
            }
        }
        else
        {
            // TODO: runs a fake event or removes the event or do nothing?
            event_t *ev;
            for(ev = events; ev != NULL; ev = ev->next)
            {
                if(event == ev->type)
                {
                    int fargc = 1;
                    JSValue* fargv = (JSValue*)malloc(sizeof(JSValue) * fargc);
                    JSValue e = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, e, "type", JS_NewUint32(ctx, ev->type));
                    fargv[0] = e;
                    JS_Call(ctx, ev->func, ev->this, fargc, fargv);
                    for(int i = 0; i<fargc; i++) JS_FreeValue(ctx, fargv[i]);
                }
            }
        }
    }
    else
    {
        // TODO: removeEventHandler
    }

    return JS_UNDEFINED;
}

static JSValue js_gfx_rgb(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    uint r,g,b,a;
    if (JS_ToUint32(ctx, &r, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &g, argv[1]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &b, argv[2]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &a, argv[3]))
        return JS_NewUint32(ctx, SDL_MapRGB(s->surface->format, r, g, b));
    else
        return JS_NewUint32(ctx, SDL_MapRGBA(s->surface->format, r, g, b, a));
}

static JSValue js_gfx_fillrect(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    uint color;
    if(argc <= 1)
    {
        if (JS_ToUint32(ctx, &color, argv[0]))
            return JS_EXCEPTION;
        return JS_NewBool(ctx, SDL_FillRect(s->surface, NULL, color) == 0);
    }
    else
    {
        SDL_Rect rect;
        if (JS_ToInt32(ctx, &rect.x, argv[0]))
            return JS_EXCEPTION;
        if (JS_ToInt32(ctx, &rect.y, argv[1]))
            return JS_EXCEPTION;
        if (JS_ToInt32(ctx, &rect.w, argv[2]))
            return JS_EXCEPTION;
        if (JS_ToInt32(ctx, &rect.h, argv[3]))
            return JS_EXCEPTION;
        if (JS_ToUint32(ctx, &color, argv[4]))
            return JS_EXCEPTION;
        return JS_NewBool(ctx, SDL_FillRect(s->surface, &rect, color) == 0);
    }
}

static JSValue js_gfx_initialize(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if (!s)
        return JS_EXCEPTION;

    int flags = SDL_INIT_VIDEO;

    if(argc > 0) JS_ToInt32(ctx, &flags, argv[0]);

    if(SDL_Init(flags) < 0)
        return JS_EXCEPTION;

    s->window = SDL_CreateWindow(s->title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, s->width, s->height, SDL_WINDOW_SHOWN);

    if(s->window==NULL)
        return JS_EXCEPTION;

    s->surface = SDL_GetWindowSurface(s->window);
    //SDL_FillRect(s->surface, NULL, SDL_MapRGB(s->surface->format, 0xFF, 0xFF, 0xFF));
    //SDL_UpdateWindowSurface(s->window);

    return JS_UNDEFINED;
}

static JSClassDef js_gfx_class = {
    "Graphics",
    .finalizer = js_gfx_finalizer,
};

static const JSCFunctionListEntry js_gfx_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("width", js_gfx_get_size, js_gfx_set_size, 0),
    JS_CGETSET_MAGIC_DEF("height", js_gfx_get_size, js_gfx_set_size, 1),
    JS_CGETSET_DEF("title", js_gfx_get_title, js_gfx_set_title),
    JS_CFUNC_DEF("initialize", 1, js_gfx_initialize),
    JS_CFUNC_DEF("delay", 1, js_gfx_delay),
    JS_CFUNC_DEF("quit", 0, js_gfx_quit),
    JS_CFUNC_MAGIC_DEF("pollEvent", 0, js_gfx_pollevent, 0),
    JS_CFUNC_MAGIC_DEF("waitEvent", 1, js_gfx_pollevent, 1),
    JS_CFUNC_MAGIC_DEF("addEventListener", 2, js_gfx_events, 1),
    JS_CFUNC_MAGIC_DEF("removeEventListener", 1, js_gfx_events, 0),
    JS_ALIAS_DEF("on", "addEventListener"),
    JS_ALIAS_DEF("off", "removeEventListener"),
    JS_CFUNC_DEF("update", 0, js_gfx_update),
    JS_CFUNC_DEF("rgb", 3, js_gfx_rgb),
    JS_CFUNC_DEF("fillRect", 1, js_gfx_fillrect),
};

static int js_gfx_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue gfx_proto, gfx_class;
    
    /* create the gfx class */
    JS_NewClassID(&js_gfx_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_gfx_class_id, &js_gfx_class);

    gfx_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, gfx_proto, js_gfx_proto_funcs, countof(js_gfx_proto_funcs));
    
    gfx_class = JS_NewCFunction2(ctx, js_gfx_ctor, "Graphics", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, gfx_class, gfx_proto);
    JS_SetClassProto(ctx, js_gfx_class_id, gfx_proto);
                      
    JS_SetModuleExport(ctx, m, "Graphics", gfx_class);
    return 0;
}

JSModuleDef *js_init_module(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_gfx_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Graphics");
    return m;
}