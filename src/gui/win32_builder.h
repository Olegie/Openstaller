#ifndef OPENSTALLER_WIN32_BUILDER_H
#define OPENSTALLER_WIN32_BUILDER_H

#if !defined(_WIN32)
#error "Openstaller builder GUI is Windows-only."
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "openstaller/openstaller.h"

#define BW_W 1120
#define BW_H 720
#define BW_RAIL_W 250
#define BW_FOOTER_Y 642
#define BW_STEP_START_Y 154
#define BW_STEP_GAP_Y 54
#define BW_THEME_COUNT 10
#define BW_ONLINE_ROWS 3

#define BW_ID_BACK 3001
#define BW_ID_NEXT 3002
#define BW_ID_CANCEL 3003
#define BW_ID_SOURCE_BROWSE 3004
#define BW_ID_OUTPUT_BROWSE 3005
#define BW_ID_LICENSE_BROWSE 3006
#define BW_ID_TEXT_PAGE 3007
#define BW_ID_OPEN_OUTPUT 3008
#define BW_ID_IMAGE_BROWSE 3009
#define BW_ID_INSTALLER_ICON_BROWSE 3010
#define BW_ID_UNINSTALLER_ICON_BROWSE 3011
#define BW_ID_BACKGROUND_IMAGE_BROWSE 3012
#define BW_ID_INSTALLER_STYLE 3013
#define BW_ID_UI_FONT 3014
#define BW_ID_WINDOW_STYLE 3015
#define BW_ID_ONLINE_COMPONENTS 3016
#define BW_ID_PAGE_FLOW 3017
#define BW_ID_THEME_COLORS 3018
#define BW_ID_THEME_RESET 3030
#define BW_ID_INSTALLER_ICON_EDIT 3031
#define BW_ID_UNINSTALLER_ICON_EDIT 3032
#define BW_ID_THEME_PICK_BASE 3100
#define BW_ID_ONLINE_PAGE_BASE 3200
#define BW_ID_PAGE_FLOW_BASE 3300

typedef enum BwPage {
    BW_PAGE_APP = 0,
    BW_PAGE_FILES = 1,
    BW_PAGE_TEXTS = 2,
    BW_PAGE_OPTIONS = 3,
    BW_PAGE_BUILD = 4,
    BW_PAGE_DONE = 5
} BwPage;

typedef struct BuilderState {
    HINSTANCE instance;
    HWND window;
    HWND title;
    HWND subtitle;
    HWND back;
    HWND next;
    HWND cancel;
    HWND app_name_label;
    HWND company_name_label;
    HWND app_version_label;
    HWND install_dir_label;
    HWND app_name;
    HWND company_name;
    HWND app_version;
    HWND install_dir;
    HWND source_dir_label;
    HWND output_dir_label;
    HWND license_file_label;
    HWND wizard_image_label;
    HWND background_image_label;
    HWND installer_icon_label;
    HWND uninstaller_icon_label;
    HWND launcher_label;
    HWND source_dir;
    HWND output_dir;
    HWND license_file;
    HWND wizard_image;
    HWND background_image;
    HWND installer_icon;
    HWND uninstaller_icon;
    HWND installer_icon_preview;
    HWND uninstaller_icon_preview;
    HWND launcher;
    HWND text_page_label;
    HWND page_title_label;
    HWND page_body_label;
    HWND text_page;
    HWND text_tabs;
    HWND page_title;
    HWND page_body;
    HWND installer_style_label;
    HWND installer_style;
    HWND ui_font_label;
    HWND ui_font;
    HWND window_style_label;
    HWND window_style;
    HWND page_flow_label;
    HWND page_flow;
    HWND page_flow_checks[6];
    HWND page_flow_count;
    HWND theme_colors_label;
    HWND theme_colors;
    HWND theme_reset;
    HWND online_components_label;
    HWND online_components;
    HWND theme_name[BW_THEME_COUNT];
    HWND theme_value[BW_THEME_COUNT];
    HWND theme_pick[BW_THEME_COUNT];
    HWND online_header[5];
    HWND online_name[BW_ONLINE_ROWS];
    HWND online_url[BW_ONLINE_ROWS];
    HWND online_target[BW_ONLINE_ROWS];
    HWND online_page[BW_ONLINE_ROWS];
    HWND online_default[BW_ONLINE_ROWS];
    HWND register_box;
    HWND native_box;
    HWND windows_box;
    HWND unix_box;
    HWND status;
    HWND open_output;
    HFONT font_body;
    HFONT font_bold;
    HFONT font_title;
    HICON icon_big;
    HICON icon_small;
    HICON installer_icon_preview_handle;
    HICON uninstaller_icon_preview_handle;
    HBRUSH brush_bg;
    HBRUSH brush_white;
    OsProjectConfig config;
    OsGenerationResult result;
    BwPage page;
    int selected_text_page;
    int generated;
} BuilderState;

extern BuilderState g_bw;

HWND bw_label(HWND parent, const char *text, int x, int y, int w, int h, HFONT font);
HWND bw_edit(HWND parent, int id, const char *text, int x, int y, int w, int h, int multiline);
HWND bw_button(HWND parent, int id, const char *text, int x, int y, int w, int h);
HWND bw_check(HWND parent, int id, const char *text, int x, int y, int w, int h, int checked);
HWND bw_combo(HWND parent, int id, int x, int y, int w, int h);
HFONT bw_font(int points, int weight);
void bw_set_font(HWND hwnd, HFONT font);
void bw_show(HWND hwnd, int visible);
void bw_create_controls(HWND hwnd);
void bw_paint(HWND hwnd);
void bw_draw_template_preview(HDC dc, const RECT *bounds);
int bw_template_preview_style_from_point(const RECT *bounds, int x, int y);
void bw_dispose_template_preview(void);

void bw_init_config(void);
void bw_set_runtime_exe(char *out, size_t out_size);
void bw_pick_source(void);
void bw_pick_output(void);
void bw_pick_license(void);
void bw_pick_wizard_image(void);
void bw_pick_background_image(void);
void bw_pick_installer_icon(void);
void bw_pick_uninstaller_icon(void);
void bw_update_icon_previews(void);
int bw_ui_font_selection(void);
void bw_save_visible_values(void);
void bw_load_text_editor(void);
void bw_save_text_editor(void);
void bw_load_theme_editor(void);
void bw_load_page_flow_editor(void);
void bw_pick_theme_color(int index);
void bw_reset_theme_colors(void);
void bw_set_page(BwPage page);
void bw_go_to_page(BwPage page);
int bw_step_from_point(int x, int y);
void bw_next(void);
void bw_back(void);

#endif
