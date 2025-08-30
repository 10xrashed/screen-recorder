#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
using namespace std;
GtkWidget *window;
GtkWidget *main_box;
GtkWidget *frame_rate_entry;
GtkWidget *file_name_entry;
GtkWidget *start_button;
GtkWidget *stop_button;
GtkWidget *status_label;
GtkWidget *region_toggle;
GtkWidget *x_entry, *y_entry, *width_entry, *height_entry;
GtkWidget *format_combo;
GtkWidget *quality_combo;
static pid_t ffmpeg_pid = 0;
bool is_valid_number(const string &str, int min_val = 0, int max_val = INT_MAX) {
    if (str.empty()) return false;
    
    try {
        int val = stoi(str);
        return val >= min_val && val <= max_val;
    } catch (const exception &e) {
        return false;
    }
}

bool is_valid_filename(const string &filename) {
    if (filename.empty()) return false;
    
    const string invalid_chars = "<>:\"|?*";
    for (char c : invalid_chars) {
        if (filename.find(c) != string::npos) {
            return false;
        }
    }
    
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == string::npos) return false;
    
    string ext = filename.substr(dot_pos + 1);
    return (ext == "mp4" || ext == "avi" || ext == "mkv" || ext == "webm");
}

bool check_ffmpeg() {
    return system("which ffmpeg > /dev/null 2>&1") == 0;
}

void get_screen_dimensions(int &width, int &height) {
    Display *d = XOpenDisplay(NULL);
    if (d) {
        Screen *s = DefaultScreenOfDisplay(d);
        width = s->width;
        height = s->height;
        XCloseDisplay(d);
    } else {
        width = 1920;
        height = 1080;
    }
}

void update_status(const string &message) {
    time_t now = time(0);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));
    
    string status_text = "[" + string(timestamp) + "] " + message;
    gtk_label_set_text(GTK_LABEL(status_label), status_text.c_str());
}

void terminate_ffmpeg() {
    if (ffmpeg_pid > 0) {
        kill(ffmpeg_pid, SIGINT);
        
        int status;
        for (int i = 0; i < 50; i++) {
            if (waitpid(ffmpeg_pid, &status, WNOHANG) > 0) {
                ffmpeg_pid = 0;
                return;
            }
            usleep(100000);
        }
        
        kill(ffmpeg_pid, SIGKILL);
        waitpid(ffmpeg_pid, &status, 0);
        ffmpeg_pid = 0;
    }
}

static void start_recording(GtkWidget *button, gpointer data) {
    const gchar *rate = gtk_entry_get_text(GTK_ENTRY(frame_rate_entry));
    const gchar *name = gtk_entry_get_text(GTK_ENTRY(file_name_entry));
    
    if (!is_valid_number(string(rate), 1, 60)) {
        update_status("Error: Frame rate must be between 1-60 fps");
        return;
    }
    
    if (!is_valid_filename(string(name))) {
        update_status("Error: Invalid filename. Use .mp4, .avi, .mkv, or .webm");
        return;
    }
    
    if (!check_ffmpeg()) {
        update_status("Error: ffmpeg not found. Please install ffmpeg");
        return;
    }
    
    string command = "ffmpeg -f x11grab -framerate " + string(rate);
    
    const gchar *quality = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(quality_combo));
    if (quality) {
        if (strcmp(quality, "High") == 0) {
            command += " -crf 18";
        } else if (strcmp(quality, "Medium") == 0) {
            command += " -crf 23";
        } else {
            command += " -crf 28";
        }
        g_free((gpointer)quality);
    }
    
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(region_toggle))) {
        const gchar *x = gtk_entry_get_text(GTK_ENTRY(x_entry));
        const gchar *y = gtk_entry_get_text(GTK_ENTRY(y_entry));
        const gchar *w = gtk_entry_get_text(GTK_ENTRY(width_entry));
        const gchar *h = gtk_entry_get_text(GTK_ENTRY(height_entry));
        
        if (!is_valid_number(string(x)) || !is_valid_number(string(y)) ||
            !is_valid_number(string(w), 1) || !is_valid_number(string(h), 1)) {
            update_status("Error: Invalid region coordinates");
            return;
        }
        
        command += " -video_size " + string(w) + "x" + string(h) + " -i :0.0+" + string(x) + "," + string(y);
    } else {
        int width, height;
        get_screen_dimensions(width, height);
        command += " -video_size " + to_string(width) + "x" + to_string(height) + " -i :0.0";
    }
    
    command += " -y \"" + string(name) + "\" > /dev/null 2>&1 & echo $!";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        update_status("Error: Failed to start recording process");
        return;
    }
    
    char buffer[128];
    string pid_str = "";
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        pid_str = buffer;
        if (!pid_str.empty() && pid_str.back() == '\n') {
            pid_str.pop_back();
        }
    }
    pclose(pipe);
    
    if (!pid_str.empty() && is_valid_number(pid_str, 1)) {
        ffmpeg_pid = stoi(pid_str);
        update_status("Recording started successfully");
        gtk_widget_set_sensitive(start_button, FALSE);
        gtk_widget_set_sensitive(stop_button, TRUE);
        gtk_widget_set_sensitive(frame_rate_entry, FALSE);
        gtk_widget_set_sensitive(file_name_entry, FALSE);
        gtk_widget_set_sensitive(region_toggle, FALSE);
        gtk_widget_set_sensitive(format_combo, FALSE);
        gtk_widget_set_sensitive(quality_combo, FALSE);
    } else {
        update_status("Error: Failed to start recording - invalid PID");
    }
}

static void stop_recording(GtkWidget *button, gpointer data) {
    if (ffmpeg_pid > 0) {
        update_status("Stopping recording...");
        terminate_ffmpeg();
        update_status("Recording stopped successfully");
        
        gtk_widget_set_sensitive(start_button, TRUE);
        gtk_widget_set_sensitive(stop_button, FALSE);
        gtk_widget_set_sensitive(frame_rate_entry, TRUE);
        gtk_widget_set_sensitive(file_name_entry, TRUE);
        gtk_widget_set_sensitive(region_toggle, TRUE);
        gtk_widget_set_sensitive(format_combo, TRUE);
        gtk_widget_set_sensitive(quality_combo, TRUE);
    }
}

static void toggle_region(GtkWidget *toggle, gpointer data) {
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
    gtk_widget_set_sensitive(x_entry, active);
    gtk_widget_set_sensitive(y_entry, active);
    gtk_widget_set_sensitive(width_entry, active);
    gtk_widget_set_sensitive(height_entry, active);
}

static void on_format_changed(GtkComboBox *combo, gpointer data) {
    const gchar *format = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if (format) {
        const gchar *current_name = gtk_entry_get_text(GTK_ENTRY(file_name_entry));
        string name_str = string(current_name);
        
        size_t dot_pos = name_str.find_last_of('.');
        if (dot_pos != string::npos) {
            name_str = name_str.substr(0, dot_pos);
        }
        
        if (strcmp(format, "MP4") == 0) {
            name_str += ".mp4";
        } else if (strcmp(format, "AVI") == 0) {
            name_str += ".avi";
        } else if (strcmp(format, "MKV") == 0) {
            name_str += ".mkv";
        } else if (strcmp(format, "WebM") == 0) {
            name_str += ".webm";
        }
        
        gtk_entry_set_text(GTK_ENTRY(file_name_entry), name_str.c_str());
        g_free((gpointer)format);
    }
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    if (ffmpeg_pid > 0) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_QUESTION,
                                                   GTK_BUTTONS_YES_NO,
                                                   "Recording is in progress. Stop recording and exit?");
        
        int response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        
        if (response == GTK_RESPONSE_YES) {
            terminate_ffmpeg();
            return FALSE;
        } else {
            return TRUE;
        }
    }
    return FALSE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    if (!check_ffmpeg()) {
        GtkWidget *error_dialog = gtk_message_dialog_new(NULL,
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_OK,
                                                        "ffmpeg not found!\n\nPlease install ffmpeg to use this application.\n"
                                                        "On Ubuntu/Debian: sudo apt install ffmpeg\n"
                                                        "On Fedora: sudo dnf install ffmpeg\n"
                                                        "On Arch: sudo pacman -S ffmpeg");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        return 1;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Advanced Screen Recorder");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 400);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "delete-event", G_CALLBACK(on_window_delete), NULL);

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *settings_frame = gtk_frame_new("Recording Settings");
    gtk_box_pack_start(GTK_BOX(main_box), settings_frame, FALSE, FALSE, 0);
    
    GtkWidget *settings_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(settings_box), 15);
    gtk_container_add(GTK_CONTAINER(settings_frame), settings_box);

    GtkWidget *frame_rate_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(settings_box), frame_rate_box, FALSE, FALSE, 0);
    
    GtkWidget *frame_rate_label = gtk_label_new("Frame Rate (fps):");
    gtk_widget_set_size_request(frame_rate_label, 120, -1);
    gtk_misc_set_alignment(GTK_MISC(frame_rate_label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(frame_rate_box), frame_rate_label, FALSE, FALSE, 0);
    
    frame_rate_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(frame_rate_entry), "1-60");
    gtk_entry_set_text(GTK_ENTRY(frame_rate_entry), "30");
    gtk_widget_set_size_request(frame_rate_entry, 100, -1);
    gtk_box_pack_start(GTK_BOX(frame_rate_box), frame_rate_entry, FALSE, FALSE, 0);

    GtkWidget *quality_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(settings_box), quality_box, FALSE, FALSE, 0);
    
    GtkWidget *quality_label = gtk_label_new("Quality:");
    gtk_widget_set_size_request(quality_label, 120, -1);
    gtk_misc_set_alignment(GTK_MISC(quality_label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(quality_box), quality_label, FALSE, FALSE, 0);
    
    quality_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(quality_combo), "High");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(quality_combo), "Medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(quality_combo), "Low");
    gtk_combo_box_set_active(GTK_COMBO_BOX(quality_combo), 1);
    gtk_box_pack_start(GTK_BOX(quality_box), quality_combo, FALSE, FALSE, 0);

    GtkWidget *format_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(settings_box), format_box, FALSE, FALSE, 0);
    
    GtkWidget *format_label = gtk_label_new("Format:");
    gtk_widget_set_size_request(format_label, 120, -1);
    gtk_misc_set_alignment(GTK_MISC(format_label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(format_box), format_label, FALSE, FALSE, 0);
    
    format_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "MP4");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "AVI");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "MKV");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "WebM");
    gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 0);
    g_signal_connect(format_combo, "changed", G_CALLBACK(on_format_changed), NULL);
    gtk_box_pack_start(GTK_BOX(format_box), format_combo, FALSE, FALSE, 0);

    GtkWidget *file_name_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(settings_box), file_name_box, FALSE, FALSE, 0);
    
    GtkWidget *file_name_label = gtk_label_new("File Name:");
    gtk_widget_set_size_request(file_name_label, 120, -1);
    gtk_misc_set_alignment(GTK_MISC(file_name_label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(file_name_box), file_name_label, FALSE, FALSE, 0);
    
    file_name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(file_name_entry), "recording.mp4");
    gtk_entry_set_text(GTK_ENTRY(file_name_entry), "recording.mp4");
    gtk_box_pack_start(GTK_BOX(file_name_box), file_name_entry, TRUE, TRUE, 0);

    GtkWidget *region_frame = gtk_frame_new("Region Capture");
    gtk_box_pack_start(GTK_BOX(main_box), region_frame, FALSE, FALSE, 0);
    
    GtkWidget *region_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(region_box), 15);
    gtk_container_add(GTK_CONTAINER(region_frame), region_box);

    region_toggle = gtk_check_button_new_with_label("Enable Region Capture (uncheck for full screen)");
    gtk_box_pack_start(GTK_BOX(region_box), region_toggle, FALSE, FALSE, 0);
    g_signal_connect(region_toggle, "toggled", G_CALLBACK(toggle_region), NULL);

    GtkWidget *region_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(region_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(region_grid), 10);
    gtk_box_pack_start(GTK_BOX(region_box), region_grid, FALSE, FALSE, 0);

    GtkWidget *x_label = gtk_label_new("X:");
    gtk_misc_set_alignment(GTK_MISC(x_label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(region_grid), x_label, 0, 0, 1, 1);
    
    x_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(x_entry), "0");
    gtk_widget_set_size_request(x_entry, 80, -1);
    gtk_grid_attach(GTK_GRID(region_grid), x_entry, 1, 0, 1, 1);
    
    GtkWidget *y_label = gtk_label_new("Y:");
    gtk_misc_set_alignment(GTK_MISC(y_label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(region_grid), y_label, 2, 0, 1, 1);
    
    y_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(y_entry), "0");
    gtk_widget_set_size_request(y_entry, 80, -1);
    gtk_grid_attach(GTK_GRID(region_grid), y_entry, 3, 0, 1, 1);
    
    GtkWidget *width_label = gtk_label_new("Width:");
    gtk_misc_set_alignment(GTK_MISC(width_label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(region_grid), width_label, 0, 1, 1, 1);
    
    width_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(width_entry), "1920");
    gtk_widget_set_size_request(width_entry, 80, -1);
    gtk_grid_attach(GTK_GRID(region_grid), width_entry, 1, 1, 1, 1);
    
    GtkWidget *height_label = gtk_label_new("Height:");
    gtk_misc_set_alignment(GTK_MISC(height_label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(region_grid), height_label, 2, 1, 1, 1);
    
    height_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(height_entry), "1080");
    gtk_widget_set_size_request(height_entry, 80, -1);
    gtk_grid_attach(GTK_GRID(region_grid), height_entry, 3, 1, 1, 1);

    gtk_widget_set_sensitive(x_entry, FALSE);
    gtk_widget_set_sensitive(y_entry, FALSE);
    gtk_widget_set_sensitive(width_entry, FALSE);
    gtk_widget_set_sensitive(height_entry, FALSE);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);
    
    start_button = gtk_button_new_with_label("🔴 Start Recording");
    g_signal_connect(start_button, "clicked", G_CALLBACK(start_recording), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), start_button, TRUE, TRUE, 0);
    
    stop_button = gtk_button_new_with_label("⏹️ Stop Recording");
    g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_recording), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), stop_button, TRUE, TRUE, 0);
    gtk_widget_set_sensitive(stop_button, FALSE);

    GtkWidget *status_frame = gtk_frame_new("Status");
    gtk_box_pack_start(GTK_BOX(main_box), status_frame, FALSE, FALSE, 0);
    
    status_label = gtk_label_new("");
    gtk_container_set_border_width(GTK_CONTAINER(status_label), 10);
    gtk_misc_set_alignment(GTK_MISC(status_label), 0, 0.5);
    gtk_container_add(GTK_CONTAINER(status_frame), status_label);
    
    update_status("Ready to record");

    gtk_widget_show_all(window);

    gtk_main();

    if (ffmpeg_pid > 0) {
        terminate_ffmpeg();
    }

    return 0;
}
