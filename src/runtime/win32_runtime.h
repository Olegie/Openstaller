#ifndef OPENSTALLER_WIN32_RUNTIME_H
#define OPENSTALLER_WIN32_RUNTIME_H

#if !defined(_WIN32)
#error "Openstaller runtime wizard is Windows-only."
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "openstaller/openstaller.h"

#define RT_ID_BACK 2001
#define RT_ID_NEXT 2002
#define RT_ID_CANCEL 2003
#define RT_ID_BROWSE 2004
#define RT_ID_ACCEPT 2005
#define RT_ID_INSTALLDIR 2006
#define RT_ID_PROGRESS 2007
#define RT_ID_MAINCOMP 2008
#define RT_ID_REGCOMP 2009
#define RT_ID_PROGRESS_TEXT 2010
#define RT_ID_PROGRESS_DETAILS 2011
#define RT_ID_PROGRESS_LOG 2012
#define RT_ID_PROGRESS_MORE 2013
#define RT_ID_ONLINE_COMPONENT_BASE 2020
#define RT_TIMER_PROGRESS 2101
#define RT_WM_OPERATION_DONE (WM_APP + 17)
#define RT_WM_PROGRESS_UPDATE (WM_APP + 18)

#define RT_W 672
#define RT_H 456
#define RT_SIDE_W 176
#define RT_FOOTER_Y 370
#define RT_MAX_PAGES 12

typedef enum RtMode {
    RT_MODE_INSTALL,
    RT_MODE_UNINSTALL
} RtMode;

typedef enum RtPageKind {
    RT_PAGE_WELCOME,
    RT_PAGE_LICENSE,
    RT_PAGE_FOLDER,
    RT_PAGE_COMPONENTS,
    RT_PAGE_READY,
    RT_PAGE_PROGRESS,
    RT_PAGE_FINISH,
    RT_PAGE_UNINSTALL_WELCOME,
    RT_PAGE_UNINSTALL_READY,
    RT_PAGE_UNINSTALL_PROGRESS,
    RT_PAGE_UNINSTALL_FINISH
} RtPageKind;

typedef struct RtState {
    HINSTANCE instance;
    HWND window;
    HWND title;
    HWND subtitle;
    HWND body;
    HWND install_dir;
    HWND browse;
    HWND license;
    HWND accept;
    HWND component_main;
    HWND component_reg;
    HWND online_components[OS_MAX_ONLINE_COMPONENTS];
    HWND progress;
    HWND progress_text;
    HWND progress_detail;
    HWND progress_log;
    HWND progress_more;
    HWND back;
    HWND next;
    HWND cancel;
    HFONT font_body;
    HFONT font_bold;
    HFONT font_title;
    HBRUSH brush_bg;
    HBRUSH brush_white;
    HBITMAP side_image;
    HBITMAP background_image;
    char self_path[OS_MAX_PATH_LEN];
    char package_dir[OS_MAX_PATH_LEN];
    char license_path[OS_MAX_PATH_LEN];
    char image_path[OS_MAX_PATH_LEN];
    char background_path[OS_MAX_PATH_LEN];
    char command_install_dir[OS_MAX_PATH_LEN];
    char operation_install_dir[OS_MAX_PATH_LEN];
    char result[OS_MAX_MESSAGE_LEN];
    char progress_action[160];
    char progress_source[OS_MAX_PATH_LEN];
    char progress_target[OS_MAX_PATH_LEN];
    char progress_log_text[4096];
    OsPackageInfo info;
    RtMode mode;
    RtPageKind page_kinds[RT_MAX_PAGES];
    int page;
    int page_count;
    int embedded_package;
    int has_license;
    int accepted_license;
    int operation_started;
    int operation_done;
    int operation_ok;
    int progress_value;
    int progress_floor;
    int progress_ceiling;
    int progress_percent;
    int progress_expanded;
    int progress_has_real_events;
    int progress_lock_ready;
    uint64_t online_component_mask;
    size_t progress_completed;
    size_t progress_total;
    CRITICAL_SECTION progress_lock;
} RtState;

extern RtState g_rt;

int rt_is_sep(char ch);
int rt_copy(char *dst, size_t dst_size, const char *src);
int rt_join(char *out, size_t out_size, const char *left, const char *right);
int rt_dirname(const char *path, char *out, size_t out_size);
const char *rt_basename(const char *path);
int rt_ieq(const char *a, const char *b);
COLORREF rt_theme_color(const char *value, COLORREF fallback);

HFONT rt_font(int points, int weight);
void rt_set_font(HWND hwnd, HFONT font);
void rt_show(HWND hwnd, int visible);
int rt_load_text_file(const char *path, HWND target);

HWND rt_label(HWND parent, const char *text, int x, int y, int w, int h, HFONT font);
HWND rt_button(HWND parent, int id, const char *text, int x, int y, int w, int h);
HWND rt_check(HWND parent, int id, const char *text, int x, int y, int w, int h, int checked);
HWND rt_edit(HWND parent, int id, const char *text, int x, int y, int w, int h);
HWND rt_license_box(HWND parent);
void rt_create_controls(HWND hwnd);
void rt_paint(HWND hwnd);
int rt_legacy_style_enabled(void);
void rt_legacy_apply_window_show(int *show_cmd);
void rt_legacy_layout(HWND hwnd);
int rt_legacy_ctlcolor_static(HDC dc, HWND control, LRESULT *result);
void rt_legacy_paint(HWND hwnd);
int rt_modern_style_enabled(void);
void rt_modern_layout(HWND hwnd);
int rt_modern_ctlcolor_static(HDC dc, HWND control, LRESULT *result);
void rt_modern_paint(HWND hwnd);
void rt_set_progress(int value, const char *text);
void rt_apply_progress_update(void);
void rt_toggle_progress_details(void);
int rt_install_dir_needs_elevation(const char *install_dir);
int rt_is_process_elevated(void);
int rt_relaunch_elevated_for_install(const char *install_dir);

void rt_set_page(void);
void rt_build_page_flow(void);
RtPageKind rt_current_page_kind(void);
int rt_find_page_kind(RtPageKind kind);
const char *rt_page_step_name(int index);
void rt_pick_folder(void);
void rt_next(void);
void rt_back(void);
void rt_run_operation(void);
void rt_progress_tick(void);
void rt_operation_done(void);

#endif
