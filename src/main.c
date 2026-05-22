#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

struct k9_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wl_list views;

	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener new_layer_surface;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;

	struct k9_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;
};

struct k9_output {
	struct wl_list link;
	struct k9_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

struct k9_view {
	struct wl_list link;
	struct k9_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
};

struct k9_layer_surface {
	struct k9_server *server;
	struct wlr_layer_surface_v1 *wlr_layer_surface;
	struct wlr_scene_layer_surface_v1 *scene_layer_surface;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener surface_commit;
};

struct k9_keyboard {
	struct wl_list link;
	struct k9_server *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

static void handle_sigchld(int sig) {
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

static void focus_view(struct k9_view *view, struct wlr_surface *surface) {
	if (view == NULL) return;
	struct k9_server *server = view->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) return;
	if (prev_surface) {
		struct wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel) wlr_xdg_toplevel_set_activated(prev_toplevel, false);
	}
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
	wl_list_remove(&view->link);
	wl_list_insert(&server->views, &view->link);
	wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	if (keyboard) wlr_seat_keyboard_notify_enter(seat, view->xdg_toplevel->base->surface,
		keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	struct k9_view *view = wl_container_of(listener, view, map);
	wl_list_insert(&view->server->views, &view->link);
	focus_view(view, view->xdg_toplevel->base->surface);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	struct k9_view *view = wl_container_of(listener, view, unmap);
	wl_list_remove(&view->link);
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	struct k9_view *view = wl_container_of(listener, view, destroy);
	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	free(view);
}

static void begin_interactive(struct k9_view *view, uint32_t edges) {
	struct k9_server *server = view->server;
	struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;
	if (!focused_surface || view->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) return;
	server->grabbed_view = view;
	server->grab_x = server->cursor->x;
	server->grab_y = server->cursor->y;
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &server->grab_geobox);
	server->grab_geobox.x += view->scene_tree->node.x;
	server->grab_geobox.y += view->scene_tree->node.y;
	server->resize_edges = edges;
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
	struct k9_view *view = wl_container_of(listener, view, request_move);
	begin_interactive(view, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	struct k9_view *view = wl_container_of(listener, view, request_resize);
	struct wlr_xdg_toplevel_resize_event *event = data;
	begin_interactive(view, event->edges);
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);
		wlr_scene_xdg_surface_create(parent->data, xdg_surface);
		return;
	}
	struct k9_view *view = calloc(1, sizeof(*view));
	view->server = server;
	view->xdg_toplevel = xdg_surface->toplevel;
	view->scene_tree = wlr_scene_xdg_surface_create(&server->scene->tree, xdg_surface);
	view->scene_tree->node.data = view;
	xdg_surface->data = view->scene_tree;
	view->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_surface->surface->events.map, &view->map);
	view->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&view->xdg_toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&view->xdg_toplevel->events.request_resize, &view->request_resize);
}

static void layer_surface_map(struct wl_listener *listener, void *data) {
	struct k9_layer_surface *k9_layer_surface = wl_container_of(listener, k9_layer_surface, map);
	wlr_surface_send_enter(k9_layer_surface->wlr_layer_surface->surface, k9_layer_surface->wlr_layer_surface->output);
}

static void layer_surface_destroy(struct wl_listener *listener, void *data) {
	struct k9_layer_surface *k9_layer_surface = wl_container_of(listener, k9_layer_surface, destroy);
	wl_list_remove(&k9_layer_surface->map.link);
	wl_list_remove(&k9_layer_surface->destroy.link);
	wl_list_remove(&k9_layer_surface->surface_commit.link);
	free(k9_layer_surface);
}

static void layer_surface_surface_commit(struct wl_listener *listener, void *data) {
	struct k9_layer_surface *k9_layer_surface = wl_container_of(listener, k9_layer_surface, surface_commit);
	struct wlr_layer_surface_v1 *wlr_layer_surface = k9_layer_surface->wlr_layer_surface;
	if (wlr_layer_surface->initial_commit) {
		wlr_layer_surface_v1_configure(wlr_layer_surface, wlr_layer_surface->pending.desired_width, wlr_layer_surface->pending.desired_height);
	}
}

static void server_new_layer_surface(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, new_layer_surface);
	struct wlr_layer_surface_v1 *wlr_layer_surface = data;
	if (!wlr_layer_surface->output) {
		if (wl_list_empty(&server->outputs)) return;
		struct k9_output *output = wl_container_of(server->outputs.next, output, link);
		wlr_layer_surface->output = output->wlr_output;
	}
	struct k9_layer_surface *k9_layer_surface = calloc(1, sizeof(*k9_layer_surface));
	k9_layer_surface->server = server;
	k9_layer_surface->wlr_layer_surface = wlr_layer_surface;
	k9_layer_surface->scene_layer_surface = wlr_scene_layer_surface_v1_create(&server->scene->tree, wlr_layer_surface);
	k9_layer_surface->map.notify = layer_surface_map;
	wl_signal_add(&wlr_layer_surface->surface->events.map, &k9_layer_surface->map);
	k9_layer_surface->destroy.notify = layer_surface_destroy;
	wl_signal_add(&wlr_layer_surface->events.destroy, &k9_layer_surface->destroy);
	k9_layer_surface->surface_commit.notify = layer_surface_surface_commit;
	wl_signal_add(&wlr_layer_surface->surface->events.commit, &k9_layer_surface->surface_commit);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	struct k9_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct k9_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		uint32_t keycode = event->keycode + 8;
		const xkb_keysym_t *syms;
		int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);
		for (int i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_Escape) { wl_display_terminate(server->wl_display); return; }
		}
	}
	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_key(server->seat, event->time_msec, event->keycode, event->state);
}

static void server_new_input(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
		struct k9_keyboard *keyboard = calloc(1, sizeof(*keyboard));
		keyboard->server = server;
		keyboard->wlr_keyboard = wlr_keyboard_from_input_device(device);
		struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
		wlr_keyboard_set_keymap(keyboard->wlr_keyboard, keymap);
		xkb_keymap_unref(keymap);
		xkb_context_unref(context);
		keyboard->key.notify = keyboard_handle_key;
		wl_signal_add(&keyboard->wlr_keyboard->events.key, &keyboard->key);
		wl_list_insert(&server->keyboards, &keyboard->link);
		wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
	} else if (device->type == WLR_INPUT_DEVICE_POINTER) {
		wlr_cursor_attach_input_device(server->cursor, device);
	}
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(server->seat, caps);
}

static void output_frame(struct wl_listener *listener, void *data) {
	struct k9_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(output->server->scene, output->wlr_output);
	if (!scene_output) return;
	wlr_scene_output_commit(scene_output, NULL);
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void server_new_output(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode) wlr_output_state_set_mode(&state, mode);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);
	struct k9_output *output = calloc(1, sizeof(*output));
	output->wlr_output = wlr_output;
	output->server = server;
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
	wlr_scene_attach_output_layout(server->scene, server->output_layout);
}

static void process_cursor_move(struct k9_server *server, uint32_t time) {
	struct k9_view *view = server->grabbed_view;
	wlr_scene_node_set_position(&view->scene_tree->node, server->cursor->x - server->grab_x + server->grab_geobox.x, server->cursor->y - server->grab_y + server->grab_geobox.y);
}

static void process_cursor_resize(struct k9_server *server, uint32_t time) {
	struct k9_view *view = server->grabbed_view;
	double dx = server->cursor->x - server->grab_x;
	double dy = server->cursor->y - server->grab_y;
	int x = server->grab_geobox.x, y = server->grab_geobox.y, width = server->grab_geobox.width, height = server->grab_geobox.height;
	if (server->resize_edges & WLR_EDGE_TOP) { y += dy; height -= dy; }
	if (server->resize_edges & WLR_EDGE_BOTTOM) { height += dy; }
	if (server->resize_edges & WLR_EDGE_LEFT) { x += dx; width -= dx; }
	if (server->resize_edges & WLR_EDGE_RIGHT) { width += dx; }
	wlr_scene_node_set_position(&view->scene_tree->node, x, y);
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, width, height);
}

static void process_cursor_motion(struct k9_server *server, uint32_t time) {
	if (server->grabbed_view) {
		if (server->resize_edges) process_cursor_resize(server, time);
		else process_cursor_move(server, time);
		return;
	}
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node, server->cursor->x, server->cursor->y, &sx, &sy);
	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
		if (scene_surface) surface = scene_surface->surface;
	}
	if (surface) wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
	else wlr_seat_pointer_clear_focus(server->seat);
	wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) server->grabbed_view = NULL;
	else {
		double sx, sy;
		struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node, server->cursor->x, server->cursor->y, &sx, &sy);
		if (node) {
			struct wlr_scene_tree *tree = node->parent;
			while (tree && tree->node.data == NULL) tree = tree->node.parent;
			if (tree && tree->node.data) focus_view((struct k9_view *)tree->node.data, NULL);
		}
	}
}

static void server_request_cursor(struct wl_listener *listener, void *data) {
	struct k9_server *server = wl_container_of(listener, server, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
}

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);
	struct k9_server server = {0};
	server.wl_display = wl_display_create();
	server.backend = wlr_backend_autocreate(server.wl_display, NULL);
	server.renderer = wlr_renderer_autocreate(server.backend);
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	wlr_compositor_create(server.wl_display, 5, server.renderer);
	wlr_subcompositor_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);
	server.output_layout = wlr_output_layout_create();
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);
	server.scene = wlr_scene_create();
	wl_list_init(&server.views);
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
	server.new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);
	server.layer_shell = wlr_layer_shell_v1_create(server.wl_display, 3);
	server.new_layer_surface.notify = server_new_layer_surface;
	wl_signal_add(&server.layer_shell->events.new_surface, &server.new_layer_surface);
	wlr_screencopy_manager_v1_create(server.wl_display);
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
	server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server.cursor_mgr, 1);
	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	server.seat = wlr_seat_create(server.wl_display, "seat0");
	server.request_cursor.notify = server_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!wlr_backend_start(server.backend)) return 1;
	setenv("WAYLAND_DISPLAY", socket, 1);
	signal(SIGCHLD, handle_sigchld);

	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
	wl_display_run(server.wl_display);
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
