#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <time.h>

static GtkWidget *control_center = NULL;

static void toggle_control_center(GtkWidget *widget, gpointer data) {
    if (gtk_widget_get_visible(control_center)) {
        gtk_widget_hide(control_center);
    } else {
        gtk_widget_show_all(control_center);
    }
}

static gboolean update_time(GtkWidget *label) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M", t);
    gtk_label_set_text(GTK_LABEL(label), buf);
    return TRUE;
}

static GtkWidget *create_control_center() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 40);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 10);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), box);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "control-center");
    gtk_widget_set_size_request(box, 300, -1);

    // User section
    GtkWidget *user_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *avatar = gtk_image_new_from_icon_name("avatar-default-symbolic", GTK_ICON_SIZE_DND);
    GtkWidget *username = gtk_label_new("wolf@k9");
    gtk_box_pack_start(GTK_BOX(user_box), avatar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(user_box), username, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), user_box, FALSE, FALSE, 0);

    // Quick Toggles
    GtkWidget *toggles = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(toggles), 8);
    gtk_grid_set_row_spacing(GTK_GRID(toggles), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(toggles), TRUE);

    GtkWidget *wifi = gtk_button_new_with_label("Wi-Fi\nOff");
    gtk_style_context_add_class(gtk_widget_get_style_context(wifi), "toggle-btn");
    GtkWidget *dark = gtk_button_new_with_label("Dark Mode\nOn");
    gtk_style_context_add_class(gtk_widget_get_style_context(dark), "toggle-btn-active");

    gtk_grid_attach(GTK_GRID(toggles), wifi, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(toggles), dark, 1, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), toggles, FALSE, FALSE, 0);

    // Media Player
    GtkWidget *media = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(media), "media-widget");
    GtkWidget *track = gtk_label_new("Live it Little (Official Video)");
    gtk_label_set_ellipsize(GTK_LABEL(track), PANGO_ELLIPSIZE_END);
    GtkWidget *media_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *prev = gtk_button_new_from_icon_name("media-skip-backward-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *play = gtk_button_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *next = gtk_button_new_from_icon_name("media-skip-forward-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(media_btns), prev, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(media_btns), play, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(media_btns), next, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(media), track, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(media), media_btns, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), media, FALSE, FALSE, 0);

    // Volume Slider
    GtkWidget *vol_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *vol_icon = gtk_image_new_from_icon_name("audio-volume-high-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *vol_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(vol_slider), FALSE);
    gtk_widget_set_hexpand(vol_slider, TRUE);
    gtk_box_pack_start(GTK_BOX(vol_box), vol_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vol_box), vol_slider, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), vol_box, FALSE, FALSE, 0);

    return window;
}

static GtkWidget *create_panel() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), 32);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), bar);
    gtk_widget_set_size_request(bar, -1, 32);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "panel");

    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *logo = gtk_label_new(" K9 ");
    gtk_box_pack_start(GTK_BOX(left), logo, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(bar), left, FALSE, FALSE, 0);

    GtkWidget *center = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *time_label = gtk_label_new(NULL);
    update_time(time_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_time, time_label);
    gtk_box_pack_start(GTK_BOX(center), time_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bar), center, TRUE, TRUE, 0);

    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *cc_btn = gtk_button_new_from_icon_name("view-more-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect(cc_btn, "clicked", G_CALLBACK(toggle_control_center), NULL);
    gtk_box_pack_end(GTK_BOX(right), cc_btn, FALSE, FALSE, 5);
    gtk_box_pack_end(GTK_BOX(bar), right, FALSE, FALSE, 0);

    return window;
}

static GtkWidget *create_dock() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, 10);

    GtkWidget *dock = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_add(GTK_CONTAINER(window), dock);
    gtk_container_set_border_width(GTK_CONTAINER(dock), 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(dock), "dock");

    const char *apps[] = {"firefox", "folder", "terminal", "settings", "music", NULL};
    for (int i = 0; apps[i]; i++) {
        GtkWidget *btn = gtk_button_new();
        GtkWidget *img = gtk_image_new_from_icon_name(apps[i], GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_container_add(GTK_CONTAINER(btn), img);
        gtk_box_pack_start(GTK_BOX(dock), btn, FALSE, FALSE, 0);
    }

    return window;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background: transparent; }"
        ".panel { background: rgba(31, 35, 53, 0.9); color: #c0caf5; font-weight: bold; }"
        ".dock { background: rgba(31, 35, 53, 0.8); border-radius: 16px; border: 1px solid #414868; }"
        ".control-center { background: rgba(31, 35, 53, 0.95); border-radius: 20px; border: 1px solid #414868; color: #c0caf5; }"
        ".toggle-btn { background: #414868; border-radius: 12px; padding: 12px; min-height: 60px; }"
        ".toggle-btn-active { background: #7aa2f7; color: #1a1b26; border-radius: 12px; padding: 12px; min-height: 60px; }"
        ".media-widget { background: rgba(122, 162, 247, 0.1); border-radius: 12px; padding: 12px; }"
        "button { background: transparent; border: none; padding: 6px; color: inherit; }"
        "button:hover { background: rgba(122, 162, 247, 0.2); border-radius: 8px; }"
        "scale trough { background: #414868; border-radius: 4px; min-height: 8px; }"
        "scale highlight { background: #7aa2f7; border-radius: 4px; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *panel = create_panel();
    gtk_widget_show_all(panel);

    GtkWidget *dock = create_dock();
    gtk_widget_show_all(dock);

    control_center = create_control_center();

    gtk_main();
    return 0;
}
