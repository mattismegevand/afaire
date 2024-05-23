#include "afaire.h"

#define DEFAULT_FILE_PANE_SIZE 150
#define MAX_FILES 100
#define MAX_FILENAME_LENGTH 256
#define BUFFER_SIZE 1024

char folder[256] = {0};

typedef struct {
    bool display;
} markdown_renderer_t;

typedef struct {
    int dirty;
    char buf[BUFFER_SIZE];
    char filename[MAX_FILENAME_LENGTH];
} editor_t;

typedef struct {
    bool display;
    bool new_file_popup;
    int selected;
    char files[MAX_FILES][MAX_FILENAME_LENGTH];
    char selected_filename[MAX_FILENAME_LENGTH];
} file_pane_t;

static struct {
    sg_pass_action pass_action;
    markdown_renderer_t markdown_renderer;
    editor_t editor;
    file_pane_t file_pane;
} state;

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

    state.pass_action = (sg_pass_action){
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.5f, 1.0f, 1.0}}};
    state.markdown_renderer = (markdown_renderer_t){
        .display = true};
    state.file_pane = (file_pane_t){
        .selected = -1, .display = true, .new_file_popup = false, .files = {{0}}, .selected_filename = {0}};
    state.editor = (editor_t){
        .dirty = -1, .buf = {0}, .filename = {0}};

    if (folder[0] == '\0') {
        printf("Usage: afaire <folder>\n");
        sapp_quit();
    }
}

static void set_dirty(int dirty, const char *filename, bool force) {
    if (force || state.editor.dirty != dirty) {
        state.editor.dirty = dirty;
        snprintf(state.file_pane.selected_filename, sizeof(state.file_pane.selected_filename), "%s%s", dirty ? "* " : "  ", filename);
    }
}

static void read_file(const char *path) {
    char fullpath[BUFFER_SIZE];
    char *filename;
    filename = state.file_pane.files[state.file_pane.selected];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    FILE *file = fopen(fullpath, "r");
    if (file) {
        memset(state.editor.buf, 0, BUFFER_SIZE);
        fread(state.editor.buf, 1, BUFFER_SIZE, file);
        fclose(file);
        set_dirty(0, filename, true);
    }
}

static void new_file(const char *path, const char *filename) {
    char fullpath[BUFFER_SIZE];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    FILE *file = fopen(fullpath, "w");
    if (file) {
        fclose(file);
        set_dirty(0, filename, true);
    }
}

static void save_file(const char *path) {
    if (state.editor.dirty < 0)
        return;
    char fullpath[BUFFER_SIZE];
    char *filename;
    filename = state.file_pane.files[state.file_pane.selected];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    FILE *file = fopen(fullpath, "w");
    if (file) {
        fwrite(state.editor.buf, 1, strlen(state.editor.buf), file);
        fclose(file);
        set_dirty(0, filename, true);
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
    int i;
    DIR *d;
    struct dirent *dir;
    for (i = 0; state.file_pane.files[i][0] != '\0'; i++)
        state.file_pane.files[i][0] = '\0';
    d = opendir(folder);
    if (d) {
        i = 0;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                strncpy(state.file_pane.files[i++], dir->d_name, MAX_FILENAME_LENGTH);
            }
        }
        closedir(d);
    }
}

static int editor_callback(ImGuiInputTextCallbackData *data) {
    set_dirty(1, state.file_pane.files[state.file_pane.selected], false);
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

    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_N))
        state.file_pane.new_file_popup = true;
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_S))
        save_file(folder);
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_Q))
        sapp_request_quit();
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_P))
        state.file_pane.display = !state.file_pane.display;
    if (igIsKeyChordPressed_Nil(ImGuiMod_Ctrl | ImGuiKey_V))
        state.markdown_renderer.display = !state.markdown_renderer.display;
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
        if (igMenuItem_Bool("New", "Ctrl+N", false, true))
            state.file_pane.new_file_popup = true;
        if (igMenuItem_Bool("Save", "Ctrl+S", false, true))
            save_file(folder);
        if (igMenuItem_Bool("Quit", "Ctrl+Q", false, true))
            sapp_request_quit();
        igEndMenu();
    }
    if (igBeginMenu("View", true)) {
        if (igMenuItem_Bool("Files", "Ctrl+P", state.file_pane.display, true))
            state.file_pane.display = !state.file_pane.display;
        if (igMenuItem_Bool("Markdown Preview", "Ctrl+V", state.markdown_renderer.display, true))
            state.markdown_renderer.display = !state.markdown_renderer.display;
        igEndMenu();
    }
    igEndMenuBar();

    if (state.file_pane.display) {
        igBeginChild_Str("files_pane", (ImVec2){DEFAULT_FILE_PANE_SIZE, -1}, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX, false);
        if (state.file_pane.files[0][0] == '\0')
            read_dir(folder);
        for (int i = 0; state.file_pane.files[i][0] != '\0'; i++) {
            bool is_current_file = strcmp(state.file_pane.files[i], state.file_pane.selected_filename + 2) == 0;
            if (igSelectable_Bool((is_current_file) ? state.file_pane.selected_filename : state.file_pane.files[i], is_current_file, 0, (ImVec2){0, 0})) {
                state.file_pane.selected = i;
                read_file(folder);
            }
            if (igBeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonRight)) {
                if (igSmallButton("Rename")) {
                    // TODO: rename function
                    igCloseCurrentPopup();
                }
                if (igSmallButton("Delete")) {
                    if (state.file_pane.selected == i) {
                        state.editor.buf[0] = '\0';
                        state.file_pane.selected = -1;
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
    if (state.markdown_renderer.display)
        avail.x /= 2;
    igInputTextMultiline("## editor", state.editor.buf, sizeof(state.editor.buf), (ImVec2){avail.x, -1}, ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackEdit, &editor_callback, 0);
    if (state.markdown_renderer.display) {
        igSameLine(0, 0);
        igBeginChild_Str("files_pane2", (ImVec2){avail.x, -1}, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX, false);
        igText("Markdown Preview");
        igEndChild();
    }
    if (igBeginPopupModal("new_file", NULL, 0)) {
        igText("New filename:");
        igSameLine(0, 0);
        char new_filename[256] = {0};
        bool enter = igInputText("##new_filename", new_filename, sizeof(new_filename), ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL);
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
    if (argc == 2)
        strncpy(folder, argv[1], sizeof(folder));
    
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
