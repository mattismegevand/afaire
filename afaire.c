#include "afaire.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef int32_t b32;
typedef int32_t i32;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef uintptr_t uptr;
typedef char byte;
typedef ptrdiff_t size;
typedef size_t usize;

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define DEFAULT_FILE_PANE_SIZE 150
#define MAX_FILES 64
#define MAX_EDITORS 16
#define MAX_STRING_LENGTH 256
#define BUFFER_SIZE 1024

char folder[MAX_STRING_LENGTH] = {'\0'};

typedef struct {
    bool display;
} markdown_renderer_t;

typedef struct {
    bool active;
    bool dirty;
    u8 editor_index;
    u8 file_index;
    char buf[BUFFER_SIZE];
    char filename[MAX_STRING_LENGTH];
    char selected_filename[MAX_STRING_LENGTH];
} editor_t;

typedef struct {
    bool display;
    bool new_file_popup;
    bool fuzzy_finder_popup;
    char files[MAX_FILES][MAX_STRING_LENGTH];
} file_pane_t;

static struct {
    char error_message[MAX_STRING_LENGTH];
    sg_pass_action pass_action;
    markdown_renderer_t markdown_renderer;
    editor_t editor[MAX_EDITORS];
    file_pane_t file_pane;
} state;

editor_t *current_editor;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){.no_default_font = true});

    ImGuiIO *io = igGetIO();
    ImFontAtlas *fonts = io->Fonts;
    ImFontAtlas_AddFontFromFileTTF(fonts, "../ibm.ttf", 18.0f, NULL, NULL);
    init_style();

    state.error_message[0] = '\0';
    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.5f, 1.0f, 1.0}}};
    state.markdown_renderer = (markdown_renderer_t){.display = true};
    state.file_pane =
        (file_pane_t){.display = true, .new_file_popup = false, .fuzzy_finder_popup = false, .files = {0}};
    for (u8 i = 0; i < MAX_EDITORS; i++) {
        state.editor[i] = (editor_t){.active = false,
                                     .dirty = false,
                                     .editor_index = i,
                                     .file_index = -1,
                                     .buf = {0},
                                     .filename = {0},
                                     .selected_filename = {0}};
    }
    current_editor = &state.editor[0];
    strncpy(current_editor->filename, "*scratch*", MAX_STRING_LENGTH);
    current_editor->active = true;

    if (folder[0] == '\0') {
        printf("Usage: afaire <folder>\n");
        sapp_quit();
    }
}

static void set_dirty(bool dirty, const char *filename, bool force) {
    if (force || current_editor->dirty != dirty) {
        current_editor->dirty = dirty;
        snprintf(current_editor->selected_filename, sizeof(current_editor->selected_filename), "%s%s",
                 dirty ? "* " : "  ", filename);
    }
}

static void read_file(const char *path) {
    FILE *file;
    char fullpath[BUFFER_SIZE];
    strncpy(current_editor->filename, state.file_pane.files[current_editor->file_index], MAX_STRING_LENGTH);
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, current_editor->filename);
    file = fopen(fullpath, "r");
    if (file) {
        memset(current_editor->buf, 0, BUFFER_SIZE);
        fread(current_editor->buf, 1, BUFFER_SIZE, file);
        fclose(file);
        set_dirty(0, current_editor->filename, true);
    } else {
        snprintf(state.error_message, sizeof(state.error_message), "Could not open file %s", current_editor->filename);
    }
}

static void new_file(const char *path, const char *filename) {
    char fullpath[BUFFER_SIZE];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    FILE *file = fopen(fullpath, "w");
    if (file) {
        fclose(file);
        set_dirty(0, filename, true);
    } else {
        snprintf(state.error_message, sizeof(state.error_message), "Could not create file %s", filename);
    }
}

static void delete_file(const char *path, int i) {
    char fullpath[BUFFER_SIZE];
    char *filename;
    filename = state.file_pane.files[i];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    remove(fullpath);
}

static void read_dir(const char *path) {
    u16 i;
    DIR *d;
    struct dirent *dir;
    for (i = 0; state.file_pane.files[i][0] != '\0'; i++) {
        state.file_pane.files[i][0] = '\0';
    }
    d = opendir(path);
    if (d) {
        i = 0;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                strncpy(state.file_pane.files[i++], dir->d_name, MAX_STRING_LENGTH);
            }
        }
        closedir(d);
    } else {
        snprintf(state.error_message, sizeof(state.error_message), "Could not open directory %s", path);
    }
}

static void save_file(const char *path) {
    if (!current_editor->dirty) {
        return;
    }
    char fullpath[BUFFER_SIZE];
    char *filename;
    filename = current_editor->filename;
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    FILE *file = fopen(fullpath, "w");
    if (file) {
        fwrite(&current_editor->buf, 1, strlen(current_editor->buf), file);
        fclose(file);
        set_dirty(0, filename, true);
        read_dir(path);
    } else {
        snprintf(state.error_message, sizeof(state.error_message), "Could not save file %s", filename);
    }
}

void open_file_handler(const char *filename) {
    u8 first_free = -1;
    for (u8 i = 0; i < MAX_EDITORS; i++) {
        if (first_free == (u8)-1 && !state.editor[i].active) {
            first_free = i;
        }
        if (strcmp(state.editor[i].filename, filename) == 0) {
            current_editor = &state.editor[i];
            current_editor->active = true;
            current_editor->file_index = i;
            read_file(folder);
            return;
        }
    }
    if (first_free != (u8)-1) {
        current_editor = &state.editor[first_free];
        current_editor->active = true;
        current_editor->file_index = first_free;
        strncpy(current_editor->filename, filename, MAX_STRING_LENGTH);
        read_file(folder);
    }
}

static int editor_callback(ImGuiInputTextCallbackData *data) {
    (void)data;
    set_dirty(1, current_editor->filename, false);
    return 0;
}

static void frame(void) {
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });

    ImGuiViewport *viewport = igGetMainViewport();
    igSetNextWindowPos(viewport->Pos, ImGuiCond_Always, (ImVec2){0, 0});
    igSetNextWindowSize(viewport->Size, ImGuiCond_Always);

    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_N)) {
        state.file_pane.new_file_popup = true;
    }
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_P)) {
        state.file_pane.fuzzy_finder_popup = true;
    }
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_S)) {
        save_file(folder);
    }
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_Q)) {
        sapp_request_quit();
    }
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_B)) {
        state.file_pane.display = !state.file_pane.display;
    }
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_V)) {
        state.markdown_renderer.display = !state.markdown_renderer.display;
    }
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_Equal)) {
        ImGuiIO *io = igGetIO();
        io->FontGlobalScale += 0.1f;
    }
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_Minus)) {
        ImGuiIO *io = igGetIO();
        io->FontGlobalScale -= 0.1f;
    }

    igBegin("afaire", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);
    igBeginMenuBar();
    if (igBeginMenu("File", true)) {
        if (igMenuItem_Bool("New", "Ctrl+N", false, true)) {
            state.file_pane.new_file_popup = true;
        }
        if (igMenuItem_Bool("Open", "Ctrl+P", false, true)) {
            state.file_pane.fuzzy_finder_popup = true;
        }
        if (igMenuItem_Bool("Save", "Ctrl+S", false, true)) {
            save_file(folder);
        }
        if (igMenuItem_Bool("Quit", "Ctrl+Q", false, true)) {
            sapp_request_quit();
        }
        igEndMenu();
    }
    if (igBeginMenu("View", true)) {
        if (igMenuItem_Bool("File Pane", "Ctrl+B", state.file_pane.display, true)) {
            state.file_pane.display = !state.file_pane.display;
        }
        if (igMenuItem_Bool("Markdown Preview", "Ctrl+V", state.markdown_renderer.display, true)) {
            state.markdown_renderer.display = !state.markdown_renderer.display;
        }
        igEndMenu();
    }
    igEndMenuBar();

    if (state.file_pane.display) {
        igBeginChild_Str(
            "files_pane", (ImVec2){DEFAULT_FILE_PANE_SIZE, -1},
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX,
            false);
        if (state.file_pane.files[0][0] == '\0') {
            read_dir(folder);
        }
        for (u16 i = 0; state.file_pane.files[i][0] != '\0'; i++) {
            bool is_current_file = strcmp(state.file_pane.files[i], current_editor->filename) == 0;
            if (igSelectable_Bool((is_current_file) ? current_editor->selected_filename : state.file_pane.files[i],
                                  is_current_file, 0, (ImVec2){0, 0})) {
                open_file_handler(state.file_pane.files[i]);
            }
            if (igBeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonRight)) {
                if (igSmallButton("Rename")) {
                    // TODO: rename function
                    igCloseCurrentPopup();
                }
                if (igSmallButton("Delete")) {
                    if (current_editor->file_index == i) {
                        current_editor->buf[0] = '\0';
                        current_editor->file_index = -1;
                    }
                    delete_file(folder, i);
                    read_dir(folder);
                    igCloseCurrentPopup();
                }
                igEndPopup();
            }
        }
        igEndChild();
        igSameLine(0, 0);
    }
    ImVec2 avail;
    igGetContentRegionAvail(&avail);
    igBeginGroup();
    if (igBeginTabBar("## tabs", ImGuiTabBarFlags_None)) {
        for (u8 i = 0; i < MAX_EDITORS; i++) {
            if (igBeginTabItem(state.editor[i].filename, &state.editor[i].active, 0)) {
                if (igIsItemClicked(0)) {
                    current_editor = &state.editor[i];
                }
                igInputTextMultiline(
                    "## editor", current_editor->buf, sizeof(current_editor->buf), (ImVec2){avail.x, -1},
                    ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackEdit, &editor_callback, 0);
                igEndTabItem();
            }
        }
        igEndTabBar();
        igEndGroup();
    }
    if (state.markdown_renderer.display) {
        igSameLine(0, 0);
        igBeginChild_Str(
            "files_pane2", (ImVec2){avail.x, -1},
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX,
            false);
        igText("Markdown Preview");
        igEndChild();
    }
    if (igBeginPopupModal("new_file", NULL, 0)) {
        igText("New filename:");
        igSameLine(0, 0);
        char new_filename[MAX_STRING_LENGTH] = {'\0'};
        bool enter = igInputText("##new_filename", new_filename, sizeof(new_filename),
                                 ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL);
        if (enter || igButton("Create", (ImVec2){0, 0})) {
            new_file(folder, new_filename);
            read_dir(folder);
            new_filename[0] = '\0';
            igCloseCurrentPopup();
        }
        igSameLine(0, 0);
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0) || igButton("Cancel", (ImVec2){0, 0})) {
            new_filename[0] = '\0';
            igCloseCurrentPopup();
        }
        igEndPopup();
    }
    if (state.file_pane.new_file_popup) {
        igOpenPopup_Str("new_file", 0);
        state.file_pane.new_file_popup = false;
    }
    static bool focus = true;
    static u16 selected = -1;
    if (state.file_pane.fuzzy_finder_popup) {
        igBegin("## fuzzy_finder", 0, 0);
        static ImGuiTextFilter filter;
        u16 i, j;

        if (igBeginListBox("## search_results", (ImVec2){0, 0})) {
            for (i = 0, j = 0; i < MAX_FILES && state.file_pane.files[i][0] != '\0'; i++) {
                if (ImGuiTextFilter_PassFilter(&filter, state.file_pane.files[i], NULL)) {
                    if (igSelectable_Bool(state.file_pane.files[i], selected == j, 0, (ImVec2){0, 0})) {
                        open_file_handler(state.file_pane.files[i]);
                        state.file_pane.fuzzy_finder_popup = false;
                    }
                    j++;
                }
            }
            igEndListBox();
        }
        if (igIsKeyPressed_Bool(ImGuiKey_UpArrow, 0)) {
            selected = max(0, selected - 1);
        }
        if (igIsKeyPressed_Bool(ImGuiKey_DownArrow, 0)) {
            selected = min(j - 1, selected + 1);
        }
        if (igIsKeyPressed_Bool(ImGuiKey_Enter, 0) && selected >= 0 && selected < j) {
            for (i = 0, j = 0; i < MAX_FILES && state.file_pane.files[i][0] != '\0'; i++) {
                if (ImGuiTextFilter_PassFilter(&filter, state.file_pane.files[i], NULL)) {
                    if (j == selected) {
                        current_editor->file_index = i;
                        break;
                    }
                    j++;
                }
            }
            open_file_handler(state.file_pane.files[i]);
            state.file_pane.fuzzy_finder_popup = false;
        }
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0)) {
            state.file_pane.fuzzy_finder_popup = false;
        }

        ImGuiTextFilter_Draw(&filter, "##search", MAX_STRING_LENGTH);
        if (focus) {
            igSetKeyboardFocusHere(-1);
            focus = false;
        }
        igEnd();
    } else {
        focus = true;
        selected = -1;
    }
    if (igBeginPopupModal("## error", NULL, 0)) {
        igText("Error: %s", state.error_message);
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0) || igButton("Close", (ImVec2){0, 0})) {
            igCloseCurrentPopup();
            state.error_message[0] = '\0';
        }
        igEndPopup();
    }
    if (state.error_message[0] != '\0') {
        igOpenPopup_Str("## error", 0);
    }

    igEnd();

    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

static void event(const sapp_event *ev) {
    simgui_handle_event(ev);
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    if (argc == 2) {
        strncpy(folder, argv[1], sizeof(folder));
    }

    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .window_title = "afaire",
        .width = 1280,
        .height = 720,
        .icon.sokol_default = true,
        .logger.func = slog_func,
        .high_dpi = true,
    };
}
