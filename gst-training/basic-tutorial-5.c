#include <string.h>
#include <gst/gst.h>
#include <gtk-3.0/gtk/gtk.h>
#include <gtk-3.0/gdk/gdk.h>
 
typedef struct _CustomData {
    GstElement* playbin;
    GtkWidget* sink_widget;
    GtkWidget* slider;
    GtkWidget* streams_list;
    GtkListStore* list_store;
    gulong slider_update_signal_id;
    GstState state;
    gint64 duration;
} CustomData;

static void on_selection_changed(GtkTreeSelection* selection, CustomData* data) {
    GtkTreeIter iter;
    GtkTreeModel* model;
    gint index;
    gchar* type;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &type, 1, &index, -1);

        if (g_strcmp0(type, "Audio") == 0) {
            g_object_set(data->playbin, "current-audio", index, NULL);
        }
        else if (g_strcmp0(type, "Video") == 0) {
            g_object_set(data->playbin, "current-video", index, NULL);
        }
        else if (g_strcmp0(type, "Text") == 0) {
            g_object_set(data->playbin, "current-text", index, NULL);
        }

        g_print("Switched to %s stream index %d\n", type, index);
        g_free(type);
    }
}

static void play_cb(GtkButton* button, CustomData* data) {
    gst_element_set_state(data->playbin, GST_STATE_PLAYING);
}

static void pause_cb(GtkButton* button, CustomData* data) {
    gst_element_set_state(data->playbin, GST_STATE_PAUSED);
}

static void stop_cb(GtkButton* button, CustomData* data) {
    gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void delete_event_cb(GtkWidget* widget, GdkEvent* event, CustomData* data) {
    stop_cb(NULL, data);
    gtk_main_quit();
}

static void slider_cb(GtkRange* range, CustomData* data) {
    gdouble value = gtk_range_get_value(GTK_RANGE(data->slider));
    gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
        (gint64)(value * GST_SECOND));
}

static void analyze_streams(CustomData* data) {
    gint i, n_video, n_audio, n_text;
    GtkTreeIter iter;

    gtk_list_store_clear(data->list_store);

    g_object_get(data->playbin, "n-video", &n_video, "n-audio", &n_audio, "n-text", &n_text, NULL);

    for (i = 0; i < n_video; i++) {
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter, 0, "Video", 1, i, 2, "Video Stream", -1);
    }
    for (i = 0; i < n_audio; i++) {
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter, 0, "Audio", 1, i, 2, "Audio Stream", -1);
    }
    for (i = 0; i < n_text; i++) {
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter, 0, "Text", 1, i, 2, "Subtitle Stream", -1);
    }
}

static void create_ui(CustomData* data) {
    GtkWidget* main_window, * main_box, * main_hbox, * controls, * play_button, * pause_button, * stop_button, * scrolled_window;
    GtkTreeSelection* selection;
    GtkCellRenderer* renderer;
    GtkTreeViewColumn* column;

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(main_window), "delete-event", G_CALLBACK(delete_event_cb), data);

    play_button = gtk_button_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(play_cb), data);

    pause_button = gtk_button_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT(pause_button), "clicked", G_CALLBACK(pause_cb), data);

    stop_button = gtk_button_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(stop_cb), data);

    data->slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    data->slider_update_signal_id = g_signal_connect(G_OBJECT(data->slider), "value-changed", G_CALLBACK(slider_cb), data);

    data->list_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
    data->streams_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(data->list_store));

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->streams_list), column);

    column = gtk_tree_view_column_new_with_attributes("ID", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->streams_list), column);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->streams_list));
    g_signal_connect(selection, "changed", G_CALLBACK(on_selection_changed), data);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), data->streams_list);
    gtk_widget_set_size_request(scrolled_window, 200, -1);

    controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(controls), play_button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(controls), pause_button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(controls), stop_button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(controls), data->slider, TRUE, TRUE, 2);

    main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(main_hbox), data->sink_widget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_hbox), scrolled_window, FALSE, FALSE, 2);

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(main_box), main_hbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), controls, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(main_window), main_box);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 800, 480);
    gtk_widget_show_all(main_window);
}

static gboolean refresh_ui(CustomData* data) {
    gint64 current = -1;
    if (data->state < GST_STATE_PAUSED) return TRUE;

    if (!GST_CLOCK_TIME_IS_VALID(data->duration)) {
        if (gst_element_query_duration(data->playbin, GST_FORMAT_TIME, &data->duration)) {
            gtk_range_set_range(GTK_RANGE(data->slider), 0, (gdouble)data->duration / GST_SECOND);
        }
    }

    if (gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current)) {
        g_signal_handler_block(data->slider, data->slider_update_signal_id);
        gtk_range_set_value(GTK_RANGE(data->slider), (gdouble)current / GST_SECOND);
        g_signal_handler_unblock(data->slider, data->slider_update_signal_id);
    }
    return TRUE;
}

static void tags_cb(GstElement* playbin, gint stream, CustomData* data) {
    gst_element_post_message(playbin, gst_message_new_application(GST_OBJECT(playbin), gst_structure_new_empty("tags-changed")));
}

static void error_cb(GstBus* bus, GstMessage* msg, CustomData* data) {
    gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void eos_cb(GstBus* bus, GstMessage* msg, CustomData* data) {
    gst_element_set_state(data->playbin, GST_STATE_READY);
}

static void application_cb(GstBus* bus, GstMessage* msg, CustomData* data) {
    if (g_strcmp0(gst_structure_get_name(gst_message_get_structure(msg)), "tags-changed") == 0) {
        analyze_streams(data);
    }
}

static void state_changed_cb(GstBus* bus, GstMessage* msg, CustomData* data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin)) {
        data->state = new_state;
        if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) refresh_ui(data);
    }
}

int main(int argc, char* argv[]) {
    CustomData data;
    GstBus* bus;
    GstElement* videosink;

    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);
    memset(&data, 0, sizeof(data));
    data.duration = GST_CLOCK_TIME_NONE;

    data.playbin = gst_element_factory_make("playbin", "playbin");
    videosink = gst_element_factory_make("gtksink", "gtksink");

    if (!data.playbin || !videosink) {
        return -1;
    }

    g_object_get(videosink, "widget", &data.sink_widget, NULL);
    g_object_set(data.playbin, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);
    g_object_set(data.playbin, "video-sink", videosink, NULL);

    g_signal_connect(data.playbin, "video-tags-changed", G_CALLBACK(tags_cb), &data);
    g_signal_connect(data.playbin, "audio-tags-changed", G_CALLBACK(tags_cb), &data);
    g_signal_connect(data.playbin, "text-tags-changed", G_CALLBACK(tags_cb), &data);

    create_ui(&data);

    bus = gst_element_get_bus(data.playbin);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(error_cb), &data);
    g_signal_connect(bus, "message::eos", G_CALLBACK(eos_cb), &data);
    g_signal_connect(bus, "message::state-changed", G_CALLBACK(state_changed_cb), &data);
    g_signal_connect(bus, "message::application", G_CALLBACK(application_cb), &data);
    gst_object_unref(bus);

    gst_element_set_state(data.playbin, GST_STATE_PLAYING);
    g_timeout_add_seconds(1, (GSourceFunc)refresh_ui, &data);

    gtk_main();

    gst_element_set_state(data.playbin, GST_STATE_NULL);
    gst_object_unref(data.playbin);

    return 0;
}