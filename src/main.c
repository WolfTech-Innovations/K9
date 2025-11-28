#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define PANEL_HEIGHT 24
#define MAX_WINDOWS 100
#define TITLE_HEIGHT 20
#define BORDER_WIDTH 2

// Dark theme colors (24-bit RGB)
#define BG_COLOR 0x1a1b26
#define PANEL_COLOR 0x1f2335
#define WINDOW_BG 0x24283b
#define BORDER_COLOR 0x414868
#define ACCENT_COLOR 0x7aa2f7
#define TEXT_COLOR 0xc0caf5

typedef struct {
    Window frame;
    Window client;
    Window title_bar;
    int x, y, width, height;
    int is_mapped;
} ManagedWindow;

// Global state
Display *display;
Window root, panel;
int screen_width, screen_height, screen;
GC gc;
Atom wm_protocols, wm_delete_window, wm_state;
ManagedWindow windows[MAX_WINDOWS];
int window_count = 0;
int running = 1;

// Utility functions
unsigned long get_color(unsigned long color) {
    return ((color & 0xFF0000) >> 16) | (color & 0x00FF00) | ((color & 0x0000FF) << 16);
}

void draw_text(Drawable d, GC gc, int x, int y, const char *text, unsigned long color) {
    XSetForeground(display, gc, get_color(color));
    XDrawString(display, d, gc, x, y, text, strlen(text));
}

void draw_panel() {
    XSetForeground(display, gc, get_color(PANEL_COLOR));
    XFillRectangle(display, panel, gc, 0, 0, screen_width, PANEL_HEIGHT);
    
    // Draw K9 logo/text
    draw_text(panel, gc, 8, 16, "K9", ACCENT_COLOR);
    
    // Draw time
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M", t);
    draw_text(panel, gc, screen_width - 60, 16, time_str, TEXT_COLOR);
    
    XFlush(display);
}

ManagedWindow* find_window_by_client(Window w) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].client == w) {
            return &windows[i];
        }
    }
    return NULL;
}

ManagedWindow* find_window_by_frame(Window w) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].frame == w || windows[i].title_bar == w) {
            return &windows[i];
        }
    }
    return NULL;
}

void frame_window(Window w) {
    if (window_count >= MAX_WINDOWS) return;
    
    XWindowAttributes attr;
    if (!XGetWindowAttributes(display, w, &attr)) return;
    
    // Don't manage override_redirect windows
    if (attr.override_redirect) return;
    
    ManagedWindow *mw = &windows[window_count++];
    mw->client = w;
    mw->x = attr.x;
    mw->y = attr.y + PANEL_HEIGHT;
    mw->width = attr.width;
    mw->height = attr.height;
    mw->is_mapped = 0;
    
    // Create frame window
    mw->frame = XCreateSimpleWindow(display, root,
                                     mw->x, mw->y,
                                     mw->width, mw->height + TITLE_HEIGHT,
                                     BORDER_WIDTH,
                                     get_color(BORDER_COLOR),
                                     get_color(WINDOW_BG));
    
    // Create title bar
    mw->title_bar = XCreateSimpleWindow(display, mw->frame,
                                         0, 0,
                                         mw->width, TITLE_HEIGHT,
                                         0,
                                         get_color(BORDER_COLOR),
                                         get_color(PANEL_COLOR));
    
    // Select events
    XSelectInput(display, mw->frame, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XSelectInput(display, mw->title_bar, ButtonPressMask | ButtonReleaseMask | ExposureMask);
    XSelectInput(display, w, PropertyChangeMask | StructureNotifyMask);
    
    // Reparent client window into frame
    XReparentWindow(display, w, mw->frame, 0, TITLE_HEIGHT);
    
    // Map windows
    XMapWindow(display, mw->title_bar);
    XMapWindow(display, mw->frame);
    
    // Set WM_STATE
    long data[] = {NormalState, None};
    XChangeProperty(display, w, wm_state, wm_state, 32,
                    PropModeReplace, (unsigned char *)data, 2);
}

void unframe_window(ManagedWindow *mw) {
    if (!mw) return;
    
    XUnmapWindow(display, mw->frame);
    XReparentWindow(display, mw->client, root, mw->x, mw->y);
    XDestroyWindow(display, mw->frame);
    
    // Remove from array
    int index = mw - windows;
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    window_count--;
}

void draw_title_bar(ManagedWindow *mw) {
    XSetForeground(display, gc, get_color(PANEL_COLOR));
    XFillRectangle(display, mw->title_bar, gc, 0, 0, mw->width, TITLE_HEIGHT);
    
    // Get window title
    char *name = NULL;
    XFetchName(display, mw->client, &name);
    if (name) {
        draw_text(mw->title_bar, gc, 6, 14, name, TEXT_COLOR);
        XFree(name);
    }
    
    // Draw close button
    XSetForeground(display, gc, get_color(0xff5555));
    XFillArc(display, mw->title_bar, gc, mw->width - 18, 4, 12, 12, 0, 360 * 64);
}

void handle_button_press(XButtonEvent *e) {
    ManagedWindow *mw = find_window_by_frame(e->window);
    if (!mw) return;
    
    // Check if close button clicked
    if (e->window == mw->title_bar && e->x >= mw->width - 18 && e->x <= mw->width - 6 &&
        e->y >= 4 && e->y <= 16) {
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.xclient.type = ClientMessage;
        msg.xclient.window = mw->client;
        msg.xclient.message_type = wm_protocols;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = wm_delete_window;
        msg.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, mw->client, False, NoEventMask, &msg);
        XFlush(display);
    }
}

void setup_wm() {
    // Set up atoms
    wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wm_state = XInternAtom(display, "WM_STATE", False);
    
    // Announce ourselves as the WM
    XSetSelectionOwner(display, XInternAtom(display, "WM_S0", False), root, CurrentTime);
    
    // Set EWMH properties
    Atom net_supported = XInternAtom(display, "_NET_SUPPORTED", False);
    Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    
    XChangeProperty(display, root, net_wm_name, utf8_string, 8,
                    PropModeReplace, (unsigned char *)"K9", 2);
    
    // Select events on root window
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | 
                 PropertyChangeMask | KeyPressMask);
    
    XSync(display, False);
}

void cleanup() {
    for (int i = 0; i < window_count; i++) {
        unframe_window(&windows[i]);
    }
    if (gc) XFreeGC(display, gc);
    if (display) XCloseDisplay(display);
}

void signal_handler(int sig) {
    running = 0;
}

int main() {
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGCHLD, SIG_IGN); // Prevent zombie processes
    
    // Open display
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    screen_width = DisplayWidth(display, screen);
    screen_height = DisplayHeight(display, screen);
    
    // Try to become the window manager
    XSetErrorHandler([](Display *d, XErrorEvent *e) {
        if (e->error_code == BadAccess) {
            fprintf(stderr, "Another window manager is already running\n");
            exit(1);
        }
        return 0;
    });
    
    setup_wm();
    
    // Create GC
    gc = XCreateGC(display, root, 0, NULL);
    
    // Create panel
    panel = XCreateSimpleWindow(display, root, 0, 0, screen_width, PANEL_HEIGHT,
                                 0, 0, get_color(PANEL_COLOR));
    XMapWindow(display, panel);
    XSelectInput(display, panel, ExposureMask);
    
    draw_panel();
    
    // Frame existing windows
    Window returned_root, returned_parent;
    Window *top_level_windows;
    unsigned int num_windows;
    
    XQueryTree(display, root, &returned_root, &returned_parent,
               &top_level_windows, &num_windows);
    
    for (unsigned int i = 0; i < num_windows; i++) {
        if (top_level_windows[i] != panel) {
            XWindowAttributes attr;
            if (XGetWindowAttributes(display, top_level_windows[i], &attr) && 
                attr.map_state == IsViewable) {
                frame_window(top_level_windows[i]);
            }
        }
    }
    XFree(top_level_windows);
    
    // Main event loop
    time_t last_time = 0;
    
    while (running) {
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            
            switch (event.type) {
                case MapRequest:
                    frame_window(event.xmaprequest.window);
                    XMapWindow(display, event.xmaprequest.window);
                    break;
                    
                case UnmapNotify: {
                    ManagedWindow *mw = find_window_by_client(event.xunmap.window);
                    if (mw && event.xunmap.event == root) {
                        unframe_window(mw);
                    }
                    break;
                }
                
                case DestroyNotify: {
                    ManagedWindow *mw = find_window_by_client(event.xdestroywindow.window);
                    if (mw) {
                        unframe_window(mw);
                    }
                    break;
                }
                
                case ConfigureRequest: {
                    XWindowChanges changes;
                    changes.x = event.xconfigurerequest.x;
                    changes.y = event.xconfigurerequest.y;
                    changes.width = event.xconfigurerequest.width;
                    changes.height = event.xconfigurerequest.height;
                    changes.border_width = event.xconfigurerequest.border_width;
                    changes.sibling = event.xconfigurerequest.above;
                    changes.stack_mode = event.xconfigurerequest.detail;
                    
                    XConfigureWindow(display, event.xconfigurerequest.window,
                                   event.xconfigurerequest.value_mask, &changes);
                    break;
                }
                
                case ButtonPress:
                    handle_button_press(&event.xbutton);
                    break;
                    
                case Expose:
                    if (event.xexpose.window == panel) {
                        draw_panel();
                    } else {
                        ManagedWindow *mw = find_window_by_frame(event.xexpose.window);
                        if (mw && event.xexpose.window == mw->title_bar) {
                            draw_title_bar(mw);
                        }
                    }
                    break;
                    
                case PropertyNotify: {
                    if (event.xproperty.atom == XA_WM_NAME) {
                        ManagedWindow *mw = find_window_by_client(event.xproperty.window);
                        if (mw) {
                            draw_title_bar(mw);
                        }
                    }
                    break;
                }
            }
        }
        
        // Update panel clock every second
        time_t now = time(NULL);
        if (now != last_time) {
            draw_panel();
            last_time = now;
        }
        
        usleep(16666); // ~60 FPS
    }
    
    cleanup();
    return 0;
}