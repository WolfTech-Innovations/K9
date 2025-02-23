#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#define PANEL_HEIGHT 30
#define MENU_WIDTH 200
#define MENU_HEIGHT 120
#define MENU_ITEM_HEIGHT 40
#define WINDOW_RADIUS 10
#define TITLE_HEIGHT 30

// MacOS-inspired colors
#define BACKGROUND_COLOR 0x1a1b26
#define PANEL_COLOR 0x1f2335
#define MENU_BG_COLOR 0x1f2335
#define WINDOW_BG_COLOR 0x2a2b36
#define WINDOW_BORDER_COLOR 0x3a3b46
#define TEXT_COLOR 0xc0caf5
#define ACCENT_COLOR 0x7aa2f7

typedef struct {
    Window win;
    int x, y, width, height;
    char *title;
    int is_focused;
} ManagedWindow;

typedef struct {
    int session_active;
    ManagedWindow windows[100];
    int window_count;
} SessionManager;

Display *display;
Window root, menu;
int screen_width, screen_height;
GC gc;
XFontStruct *font;
int menu_visible = 0;
Picture root_picture;
Picture blur_picture;
SessionManager session;

void register_as_de() {
    XChangeProperty(display, root, XInternAtom(display, "_NET_WM_NAME", False), XInternAtom(display, "UTF8_STRING", False), 8, PropModeReplace, (unsigned char *)"K9", strlen("K9"));
}

void show_system_info() {
    Window info_win = XCreateSimpleWindow(display, root, screen_width / 2 - 150, screen_height / 2 - 100, 300, 200, 1, WINDOW_BORDER_COLOR, WINDOW_BG_COLOR);
    XMapWindow(display, info_win);
    GC info_gc = XCreateGC(display, info_win, 0, NULL);
    XSetForeground(display, info_gc, TEXT_COLOR);
    XDrawString(display, info_win, info_gc, 20, 50, "System Info", 11);
    XDrawString(display, info_win, info_gc, 20, 80, "Desktop Environment: K9", 24);
    XFlush(display);
}

void create_session() {
    session.session_active = 1;
    session.window_count = 0;
}

void save_session() {
    FILE *file = fopen("session.dat", "wb");
    fwrite(&session, sizeof(SessionManager), 1, file);
    fclose(file);
}

void load_session() {
    FILE *file = fopen("session.dat", "rb");
    if (file) {
        fread(&session, sizeof(SessionManager), 1, file);
        fclose(file);
    }
}

void create_rounded_rect(Display *dpy, Window win, int width, int height, int radius) {
    XGCValues xgcv;
    GC shape_gc = XCreateGC(dpy, win, 0, &xgcv);
    Pixmap mask = XCreatePixmap(dpy, win, width, height, 1);
    
    XSetForeground(dpy, shape_gc, 0);
    XFillRectangle(dpy, mask, shape_gc, 0, 0, width, height);
    XSetForeground(dpy, shape_gc, 1);
    
    XFillArc(dpy, mask, shape_gc, 0, 0, radius * 2, radius * 2, 0, 360 * 64);
    XFillArc(dpy, mask, shape_gc, width - radius * 2, 0, radius * 2, radius * 2, 0, 360 * 64);
    XFillArc(dpy, mask, shape_gc, 0, height - radius * 2, radius * 2, radius * 2, 0, 360 * 64);
    XFillArc(dpy, mask, shape_gc, width - radius * 2, height - radius * 2, radius * 2, radius * 2, 0, 360 * 64);
    
    XFillRectangle(dpy, mask, shape_gc, radius, 0, width - radius * 2, height);
    XFillRectangle(dpy, mask, shape_gc, 0, radius, width, height - radius * 2);
    
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    
    XFreePixmap(dpy, mask);
    XFreeGC(dpy, shape_gc);
}

void create_window_decorations(ManagedWindow *mw) {
    mw->win = XCreateSimpleWindow(display, root, mw->x, mw->y, mw->width, mw->height, 1, WINDOW_BORDER_COLOR, WINDOW_BG_COLOR);
    create_rounded_rect(display, mw->win, mw->width, mw->height, WINDOW_RADIUS);
    XMapWindow(display, mw->win);
}

void draw_k9_branding() {
    XSetForeground(display, gc, ACCENT_COLOR);
    XDrawString(display, root, gc, 10, screen_height - (PANEL_HEIGHT/2) + 5, "K9 Desktop", 10);
}

void create_blur_background() {
    XRenderPictureAttributes pa;
    XRenderPictFormat *format = XRenderFindVisualFormat(display, DefaultVisual(display, DefaultScreen(display)));
    root_picture = XRenderCreatePicture(display, root, format, 0, &pa);
    blur_picture = XRenderCreatePicture(display, root, format, 0, &pa);
}

int main() {
    display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);
    screen_width = DisplayWidth(display, DefaultScreen(display));
    screen_height = DisplayHeight(display, DefaultScreen(display));
    
    create_session();
    load_session();
    register_as_de();
    create_blur_background();
    
    while (1) {
        XEvent event;
        XNextEvent(display, &event);
        
        switch (event.type) {
            case KeyPress:
                if (event.xkey.state & Mod4Mask && event.xkey.keycode == XKeysymToKeycode(display, XK_I)) {
                    show_system_info();
                }
                break;
            case Expose:
                draw_k9_branding();
                break;
            case CreateNotify:
                if (session.window_count < 100) {
                    ManagedWindow mw;
                    mw.x = event.xcreatewindow.x;
                    mw.y = event.xcreatewindow.y;
                    mw.width = event.xcreatewindow.width;
                    mw.height = event.xcreatewindow.height;
                    mw.is_focused = 0;
                    create_window_decorations(&mw);
                    session.windows[session.window_count++] = mw;
                }
                break;
        }
        usleep(1000000/60);
    }
    save_session();
    return 0;
}
