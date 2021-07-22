#include <string.h>
#include <quickjs/quickjs.h>
#include <SDL2/SDL.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    SDL_Window* window;
    SDL_Surface* surface;
    int width;
    int height;
    int x;
    int y;
    char* title;
    uint fullscreen;
    float opacity;
    SDL_bool resizable;
    SDL_bool borders;
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
    s->window = NULL;
    s->surface = NULL;
    s->fullscreen = 0;
    s->x = SDL_WINDOWPOS_UNDEFINED;
    s->y = SDL_WINDOWPOS_UNDEFINED;
    s->opacity = 1.0;
    s->resizable = SDL_FALSE;
    s->borders = SDL_TRUE;
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

static JSValue js_gfx_get_pos(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if (!s)
        return JS_EXCEPTION;
    if(s->window != NULL)
        SDL_GetWindowPosition(s->window, &s->x, &s->y);
    if (magic == 0)
        return JS_NewInt32(ctx, s->x);
    else
        return JS_NewInt32(ctx, s->y);
}

static JSValue js_gfx_set_pos(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    int v;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &v, val))
        return JS_EXCEPTION;
    if (magic == 0)
        s->x = v;
    else
        s->y = v;
    if(s->window != NULL)
        SDL_SetWindowPosition(s->window, s->x, s->y);
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
    int subtype;
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
            JS_SetPropertyStr(ctx, ev, "scan", JS_NewString(ctx, SDL_GetScancodeName(e.key.keysym.scancode)));
            JS_SetPropertyStr(ctx, ev, "key", JS_NewString(ctx, SDL_GetKeyName(e.key.keysym.sym)));
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
        case SDL_CONTROLLERAXISMOTION:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.caxis.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.caxis.which));
            JS_SetPropertyStr(ctx, ev, "axis", JS_NewUint32(ctx, e.caxis.axis));
            JS_SetPropertyStr(ctx, ev, "state", JS_NewInt32(ctx, e.caxis.value));
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.cbutton.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.cbutton.which));
            JS_SetPropertyStr(ctx, ev, "button", JS_NewUint32(ctx, e.cbutton.button));
            JS_SetPropertyStr(ctx, ev, "state", JS_NewBool(ctx, e.cbutton.state != 0));
            break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERDEVICEREMAPPED:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.cdevice.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.cdevice.which));
            break;
        case SDL_JOYAXISMOTION:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.jaxis.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.jaxis.which));
            JS_SetPropertyStr(ctx, ev, "axis", JS_NewUint32(ctx, e.jaxis.axis));
            JS_SetPropertyStr(ctx, ev, "value", JS_NewInt32(ctx, e.jaxis.value));
            break;
        case SDL_JOYBALLMOTION:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.jball.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.jball.which));
            JS_SetPropertyStr(ctx, ev, "ball", JS_NewUint32(ctx, e.jball.ball));
            JS_SetPropertyStr(ctx, ev, "xrel", JS_NewInt32(ctx, e.jball.xrel));
            JS_SetPropertyStr(ctx, ev, "yrel", JS_NewInt32(ctx, e.jball.yrel));
            break;
        case SDL_JOYHATMOTION:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.jhat.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.jhat.which));
            JS_SetPropertyStr(ctx, ev, "hat", JS_NewUint32(ctx, e.jhat.hat));
            JS_SetPropertyStr(ctx, ev, "value", JS_NewUint32(ctx, e.jhat.value));
            break;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.jbutton.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.jbutton.which));
            JS_SetPropertyStr(ctx, ev, "button", JS_NewUint32(ctx, e.jbutton.button));
            JS_SetPropertyStr(ctx, ev, "state", JS_NewBool(ctx, e.jbutton.state != 0));
            break;
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.jdevice.timestamp));
            JS_SetPropertyStr(ctx, ev, "which", JS_NewInt32(ctx, e.jdevice.which));
            break;
        case SDL_QUIT:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.quit.timestamp));
            break;
        case SDL_WINDOWEVENT:
            JS_SetPropertyStr(ctx, ev, "timestamp", JS_NewUint32(ctx, e.window.timestamp));
            JS_SetPropertyStr(ctx, ev, "windowID", JS_NewUint32(ctx, e.window.windowID));
            JS_SetPropertyStr(ctx, ev, "event", JS_NewUint32(ctx, e.window.event));
            switch(e.window.event)
            {
                case SDL_WINDOWEVENT_MOVED:
                    JS_SetPropertyStr(ctx, ev, "x", JS_NewInt32(ctx, e.window.data1));
                    JS_SetPropertyStr(ctx, ev, "y", JS_NewInt32(ctx, e.window.data2));
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    JS_SetPropertyStr(ctx, ev, "width", JS_NewInt32(ctx, e.window.data1));
                    JS_SetPropertyStr(ctx, ev, "height", JS_NewInt32(ctx, e.window.data2));
                    break;
                default:
                    break;
            }
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
        if((e.type == ev->type && e.type != SDL_WINDOWEVENT) || (e.type == SDL_WINDOWEVENT && e.window.event == ev->subtype))
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

    int event = 0, subevent = 0;
    if(strcmp(name, "keydown") == 0) event = SDL_KEYDOWN;
    if(strcmp(name, "keyup") == 0) event = SDL_KEYUP;
    if(strcmp(name, "mousemove") == 0) event = SDL_MOUSEMOTION;
    if(strcmp(name, "mousedown") == 0) event = SDL_MOUSEBUTTONDOWN;
    if(strcmp(name, "mouseup") == 0) event = SDL_MOUSEBUTTONUP;
    if(strcmp(name, "mousewheel") == 0) event = SDL_MOUSEWHEEL;
    if(strcmp(name, "controlleraxismove") == 0) event = SDL_CONTROLLERAXISMOTION;
    if(strcmp(name, "controllerbtndown") == 0) event = SDL_CONTROLLERBUTTONDOWN;
    if(strcmp(name, "controllerbtnup") == 0) event = SDL_CONTROLLERBUTTONUP;
    if(strcmp(name, "controlleradd") == 0) event = SDL_CONTROLLERDEVICEADDED;
    if(strcmp(name, "controllerremove") == 0) event = SDL_CONTROLLERDEVICEREMOVED;
    if(strcmp(name, "controllerremap") == 0) event = SDL_CONTROLLERDEVICEREMAPPED;
    if(strcmp(name, "joyaxismove") == 0) event = SDL_JOYAXISMOTION;
    if(strcmp(name, "joyballmove") == 0) event = SDL_JOYBALLMOTION;
    if(strcmp(name, "joyhatmove") == 0) event = SDL_JOYHATMOTION;
    if(strcmp(name, "joybtndown") == 0) event = SDL_JOYBUTTONDOWN;
    if(strcmp(name, "joybtnup") == 0) event = SDL_JOYBUTTONUP;
    if(strcmp(name, "joyadd") == 0) event = SDL_JOYDEVICEADDED;
    if(strcmp(name, "joyremove") == 0) event = SDL_JOYDEVICEREMOVED;
    if(strcmp(name, "quit") == 0) event = SDL_QUIT;
    if(strcmp(name, "show") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_SHOWN; }
    if(strcmp(name, "hide") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_HIDDEN; }
    if(strcmp(name, "expose") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_EXPOSED; }
    if(strcmp(name, "move") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_MOVED; }
    if(strcmp(name, "resize") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_RESIZED; }
    if(strcmp(name, "sizechange") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_SIZE_CHANGED; }
    if(strcmp(name, "minimize") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_MINIMIZED; }
    if(strcmp(name, "maximize") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_MAXIMIZED; }
    if(strcmp(name, "restore") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_RESTORED; }
    if(strcmp(name, "focus") == 0 || strcmp(name, "enter") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_ENTER; }
    if(strcmp(name, "blur") == 0 || strcmp(name, "leave") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_LEAVE; }
    if(strcmp(name, "keyfocus") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_FOCUS_GAINED; }
    if(strcmp(name, "keyblur") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_FOCUS_LOST; }
    if(strcmp(name, "close") == 0) { event = SDL_WINDOWEVENT; subevent = SDL_WINDOWEVENT_CLOSE; }

    if(event == 0) return JS_EXCEPTION;

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
                ev->next->subtype = subevent;
                ev->next->func = argv[1];
                ev->next->this = this_val;
                ev->next->next = NULL;
            }
            else
            {
                events = (event_t*)malloc(sizeof(event_t));
                //if(ev->next == NULL) return JS_EXCEPTION;
                events->type = event;
                events->subtype = subevent;
                events->func = argv[1];
                events->this = this_val;
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
    JSValue vr,vg,vb,va;
    uint r,g,b,a;
    if(!JS_IsObject(argv[0]))
    {
        if(argc < 3) return JS_EXCEPTION;
        vr = argv[0];
        vg = argv[1];
        vb = argv[2];
        va = JS_UNDEFINED;
        if(argc >= 4) va = argv[3];
    }
    else
    {
        vr = JS_GetPropertyStr(ctx, argv[0], "r");
        vg = JS_GetPropertyStr(ctx, argv[0], "g");
        vb = JS_GetPropertyStr(ctx, argv[0], "b");
        va = JS_GetPropertyStr(ctx, argv[0], "a");
    }
    if (JS_ToUint32(ctx, &r, vr))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &g, vg))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &b, vb))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &a, va))
        return JS_NewUint32(ctx, SDL_MapRGB(s->surface->format, r, g, b));
    else
        return JS_NewUint32(ctx, SDL_MapRGBA(s->surface->format, r, g, b, a));
}


static JSValue js_gfx_torgb(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    uint pixel; u_char r, g, b, a;
    if (JS_ToUint32(ctx, &pixel, argv[0]))
        return JS_EXCEPTION;
    SDL_GetRGBA(pixel, s->surface->format, &r, &g, &b, &a);
    JSValue rgb = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rgb, "r", JS_NewUint32(ctx, r));
    JS_SetPropertyStr(ctx, rgb, "g", JS_NewUint32(ctx, g));
    JS_SetPropertyStr(ctx, rgb, "b", JS_NewUint32(ctx, b));
    JS_SetPropertyStr(ctx, rgb, "a", JS_NewUint32(ctx, a));
    return rgb;
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

static JSValue js_gfx_ticks(JSContext *ctx, JSValueConst this_val)
{
    return JS_NewUint32(ctx, SDL_GetTicks());
}

static JSValue js_gfx_windowid(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if(s->window != NULL)
        return JS_NewUint32(ctx, SDL_GetWindowID(s->window));
    else
        return JS_UNDEFINED;
}

static JSValue js_gfx_bytespp(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    return JS_NewUint32(ctx, s->surface->format->BytesPerPixel);
}

static JSValue js_gfx_bitspp(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    return JS_NewUint32(ctx, s->surface->format->BitsPerPixel);
}

static JSValue js_gfx_get_fullscreen(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    return JS_NewUint32(ctx, s->fullscreen);
}

static JSValue js_gfx_set_fullscreen(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    uint flags;

    if(JS_ToUint32(ctx, &flags, val)) return JS_EXCEPTION;

    if(s->window != NULL)
    {
        if(SDL_SetWindowFullscreen(s->window, flags) == 0)
            s->fullscreen = flags;
    }
    else
        s->fullscreen = flags;
    return JS_UNDEFINED;
}

static JSValue js_gfx_get_opacity(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    if(s->window != NULL)
        SDL_GetWindowOpacity(s->window, &s->opacity);
    return JS_NewFloat64(ctx, s->opacity);
}

static JSValue js_gfx_set_opacity(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    double f;

    if(JS_ToFloat64(ctx, &f, val)) return JS_EXCEPTION;

    if(s->window != NULL)
    {
        if(SDL_SetWindowOpacity(s->window, f) == 0)
            s->fullscreen = f;
    }
    else
        s->fullscreen = f;
    return JS_UNDEFINED;
}

static JSValue js_gfx_get_resizable(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    return JS_NewBool(ctx, s->resizable);
}

static JSValue js_gfx_set_resizable(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    SDL_bool f = JS_ToBool(ctx, val);

    if(s->window != NULL)
        SDL_SetWindowResizable(s->window, f);
    s->resizable = f;

    return JS_UNDEFINED;
}

static JSValue js_gfx_get_borders(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    return JS_NewBool(ctx, s->borders);
}

static JSValue js_gfx_set_borders(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    SDL_bool f = JS_ToBool(ctx, val);

    if(s->window != NULL)
        SDL_SetWindowBordered(s->window, f);
    s->borders = f;

    return JS_UNDEFINED;
}

static JSValue js_gfx_get_pixels(JSContext *ctx, JSValueConst this_val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    //SDL_LockSurface(s->surface);
    JSValue pixels = JS_NewArrayBuffer(ctx, s->surface->pixels, s->surface->h * s->surface->pitch, NULL, NULL, 0);
    //SDL_UnlockSurface(s->surface);
    return pixels;
}

static JSValue js_gfx_set_pixels(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSGfxData *s = JS_GetOpaque2(ctx, this_val, js_gfx_class_id);
    //SDL_LockSurface(s->surface);
    size_t size; // s->surface->h * s->surface->pitch
    u_char* buf = JS_GetArrayBuffer(ctx, &size, val);
    if(size > s->surface->h * s->surface->pitch) size = s->surface->h * s->surface->pitch;
    SDL_memcpy(s->surface->pixels, buf, size);
    //SDL_UnlockSurface(s->surface);
    return JS_UNDEFINED;
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

    s->window = SDL_CreateWindow(s->title, s->x, s->y, s->width, s->height, SDL_WINDOW_SHOWN);

    if(s->window==NULL)
        return JS_EXCEPTION;

    s->surface = SDL_GetWindowSurface(s->window);
    SDL_SetWindowFullscreen(s->window, s->fullscreen);
    SDL_SetWindowOpacity(s->window, s->opacity);
    SDL_SetWindowResizable(s->window, s->resizable);
    SDL_SetWindowBordered(s->window, s->borders);

    return JS_UNDEFINED;
}

static JSClassDef js_gfx_class = {
    "Graphics",
    .finalizer = js_gfx_finalizer,
};

static const JSCFunctionListEntry js_gfx_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("width", js_gfx_get_size, js_gfx_set_size, 0),
    JS_CGETSET_MAGIC_DEF("height", js_gfx_get_size, js_gfx_set_size, 1),
    JS_CGETSET_MAGIC_DEF("x", js_gfx_get_pos, js_gfx_set_pos, 0),
    JS_CGETSET_MAGIC_DEF("y", js_gfx_get_pos, js_gfx_set_pos, 1),
    JS_CGETSET_DEF("title", js_gfx_get_title, js_gfx_set_title),
    JS_CGETSET_DEF("fullscreen", js_gfx_get_fullscreen, js_gfx_set_fullscreen),
    JS_CGETSET_DEF("opacity", js_gfx_get_opacity, js_gfx_set_opacity),
    JS_CGETSET_DEF("resizable", js_gfx_get_resizable, js_gfx_set_resizable),
    JS_CGETSET_DEF("borders", js_gfx_get_borders, js_gfx_set_borders),
    JS_CGETSET_DEF("pixels", js_gfx_get_pixels, js_gfx_set_pixels),
    JS_CGETSET_DEF("ticks", js_gfx_ticks, NULL),
    JS_CGETSET_DEF("windowID", js_gfx_windowid, NULL),
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
    JS_CFUNC_DEF("rgb", 1, js_gfx_rgb),
    JS_CFUNC_DEF("torgb", 1, js_gfx_torgb),
    JS_CFUNC_DEF("fillRect", 1, js_gfx_fillrect),
    JS_CGETSET_DEF("bytesperpixel", js_gfx_bytespp, NULL),
    JS_CGETSET_DEF("bitsperpixel", js_gfx_bitspp, NULL),
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