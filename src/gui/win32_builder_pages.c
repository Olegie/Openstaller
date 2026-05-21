#include "win32_builder.h"
#include "win32_i18n.h"

#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void bw_get(HWND hwnd, char *out, size_t out_size)
{
    if (out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (hwnd == NULL) {
        return;
    }
    GetWindowTextA(hwnd, out, (int)out_size);
    out[out_size - 1] = '\0';
}

static void bw_trim(char *text)
{
    char *start = text;
    size_t len;

    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    len = strlen(text);
    while (len > 0 &&
           (text[len - 1] == ' ' ||
            text[len - 1] == '\t' ||
            text[len - 1] == '\r' ||
            text[len - 1] == '\n')) {
        text[--len] = '\0';
    }
}

static int bw_copy_component_text(char *dst, size_t dst_size, const char *src)
{
    size_t len = strlen(src);

    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
    return src[len] == '\0' ? 0 : -1;
}

static int bw_ieq(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return 0;
        }
        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static int bw_default_token_is_selected(const char *token)
{
    return token == NULL ||
           token[0] == '\0' ||
           strcmp(token, "1") == 0 ||
           bw_ieq(token, "yes") ||
           bw_ieq(token, "true") ||
           bw_ieq(token, "checked") ||
           bw_ieq(token, "default");
}

static const char *bw_online_page_from_index(int index)
{
    switch (index) {
    case 1:
        return "ready";
    case 2:
        return "install";
    default:
        return "components";
    }
}

static int bw_online_page_to_index(const char *page)
{
    if (page != NULL && bw_ieq(page, "ready")) {
        return 1;
    }
    if (page != NULL &&
        (bw_ieq(page, "install") || bw_ieq(page, "silent") || bw_ieq(page, "end"))) {
        return 2;
    }
    return 0;
}

static void bw_parse_online_editor(void)
{
    int row;

    memset(g_bw.config.online_components, 0, sizeof(g_bw.config.online_components));
    g_bw.config.online_component_count = 0;

    for (row = 0; row < BW_ONLINE_ROWS && g_bw.config.online_component_count < OS_MAX_ONLINE_COMPONENTS; ++row) {
        char name[OS_MAX_NAME_LEN];
        char url[OS_MAX_URL_LEN];
        char target[OS_MAX_PATH_LEN];
        OsOnlineComponent *component;

        bw_get(g_bw.online_name[row], name, sizeof(name));
        bw_get(g_bw.online_url[row], url, sizeof(url));
        bw_get(g_bw.online_target[row], target, sizeof(target));
        bw_trim(name);
        bw_trim(url);
        bw_trim(target);
        if (url[0] == '\0') {
            continue;
        }

        component = &g_bw.config.online_components[g_bw.config.online_component_count++];
        if (name[0] == '\0') {
            snprintf(name, sizeof(name), "Download %llu", (unsigned long long)g_bw.config.online_component_count);
        }
        bw_copy_component_text(component->name, sizeof(component->name), name);
        bw_copy_component_text(component->url, sizeof(component->url), url);
        bw_copy_component_text(component->target_path, sizeof(component->target_path), target);
        bw_copy_component_text(component->page,
                               sizeof(component->page),
                               bw_online_page_from_index((int)SendMessageA(g_bw.online_page[row], CB_GETCURSEL, 0, 0)));
        component->selected_by_default = SendMessageA(g_bw.online_default[row], BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
}

static void bw_load_online_editor(void)
{
    int row;

    for (row = 0; row < BW_ONLINE_ROWS; ++row) {
        SetWindowTextA(g_bw.online_name[row], "");
        SetWindowTextA(g_bw.online_url[row], "");
        SetWindowTextA(g_bw.online_target[row], "");
        SendMessageA(g_bw.online_page[row], CB_SETCURSEL, 0, 0);
        SendMessageA(g_bw.online_default[row], BM_SETCHECK, BST_CHECKED, 0);
    }
    for (row = 0; row < BW_ONLINE_ROWS &&
                  row < (int)g_bw.config.online_component_count &&
                  row < OS_MAX_ONLINE_COMPONENTS;
         ++row) {
        const OsOnlineComponent *component = &g_bw.config.online_components[row];

        if (component->url[0] == '\0') {
            continue;
        }
        SetWindowTextA(g_bw.online_name[row], component->name);
        SetWindowTextA(g_bw.online_url[row], component->url);
        SetWindowTextA(g_bw.online_target[row], component->target_path);
        SendMessageA(g_bw.online_page[row], CB_SETCURSEL, bw_online_page_to_index(component->page), 0);
        SendMessageA(g_bw.online_default[row], BM_SETCHECK, component->selected_by_default ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

static uint32_t bw_page_flag_by_index(int index)
{
    switch (index) {
    case 0:
        return OS_PAGE_WELCOME;
    case 1:
        return OS_PAGE_LICENSE;
    case 2:
        return OS_PAGE_FOLDER;
    case 3:
        return OS_PAGE_COMPONENTS;
    case 4:
        return OS_PAGE_READY;
    case 5:
        return OS_PAGE_FINISH;
    default:
        return 0;
    }
}

static int bw_page_flow_count(uint32_t flags)
{
    int i;
    int count = 0;

    for (i = 0; i < 6; ++i) {
        if ((flags & bw_page_flag_by_index(i)) != 0) {
            ++count;
        }
    }
    return count;
}

void bw_load_page_flow_editor(void)
{
    uint32_t flags = g_bw.config.page_flags != 0 ? g_bw.config.page_flags : OS_PAGE_DEFAULT;
    char text[64];
    int count;
    int i;

    for (i = 0; i < 6; ++i) {
        SendMessageA(g_bw.page_flow_checks[i],
                     BM_SETCHECK,
                     (flags & bw_page_flag_by_index(i)) != 0 ? BST_CHECKED : BST_UNCHECKED,
                     0);
    }

    count = bw_page_flow_count(flags);
    snprintf(text, sizeof(text), "%d page%s enabled", count, count == 1 ? "" : "s");
    SetWindowTextA(g_bw.page_flow_count, text);
}

static void bw_parse_page_flow_editor(void)
{
    uint32_t flags = 0;
    int i;

    for (i = 0; i < 6; ++i) {
        if (SendMessageA(g_bw.page_flow_checks[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            flags |= bw_page_flag_by_index(i);
        }
    }

    g_bw.config.page_flags = flags != 0 ? flags : OS_PAGE_DEFAULT;
}

static char *bw_theme_slot(const char *name)
{
    if (bw_ieq(name, "accent")) {
        return g_bw.config.theme.accent;
    }
    if (bw_ieq(name, "progress")) {
        return g_bw.config.theme.progress;
    }
    if (bw_ieq(name, "sidebar")) {
        return g_bw.config.theme.sidebar;
    }
    if (bw_ieq(name, "sidebar_dark") || bw_ieq(name, "side_dark")) {
        return g_bw.config.theme.sidebar_dark;
    }
    if (bw_ieq(name, "background") || bw_ieq(name, "bg")) {
        return g_bw.config.theme.background;
    }
    if (bw_ieq(name, "panel") || bw_ieq(name, "card")) {
        return g_bw.config.theme.panel;
    }
    if (bw_ieq(name, "text")) {
        return g_bw.config.theme.text;
    }
    if (bw_ieq(name, "muted") || bw_ieq(name, "muted_text")) {
        return g_bw.config.theme.muted_text;
    }
    if (bw_ieq(name, "legacy_top")) {
        return g_bw.config.theme.legacy_top;
    }
    if (bw_ieq(name, "legacy_bottom")) {
        return g_bw.config.theme.legacy_bottom;
    }
    return NULL;
}

static char *bw_theme_slot_by_index(int index)
{
    switch (index) {
    case 0:
        return g_bw.config.theme.accent;
    case 1:
        return g_bw.config.theme.progress;
    case 2:
        return g_bw.config.theme.sidebar;
    case 3:
        return g_bw.config.theme.sidebar_dark;
    case 4:
        return g_bw.config.theme.background;
    case 5:
        return g_bw.config.theme.panel;
    case 6:
        return g_bw.config.theme.text;
    case 7:
        return g_bw.config.theme.muted_text;
    case 8:
        return g_bw.config.theme.legacy_top;
    case 9:
        return g_bw.config.theme.legacy_bottom;
    default:
        return NULL;
    }
}

static const char *bw_default_theme_value(int style, int index)
{
    static const char *classic[BW_THEME_COUNT] = {
        "#0078D4", "#0078D4", "#154E9E", "#082766", "#F6F8FB",
        "#FFFFFF", "#000000", "#57606A", "#0012E8", "#000012"
    };
    static const char *modern[BW_THEME_COUNT] = {
        "#2563EB", "#0EA5E9", "#111827", "#0B1220", "#F8FAFC",
        "#FFFFFF", "#0F172A", "#64748B", "#0012E8", "#000012"
    };
    static const char *legacy[BW_THEME_COUNT] = {
        "#C0C0C0", "#B0008D", "#0000C0", "#000050", "#000012",
        "#EEEFE0", "#000000", "#3A3A3A", "#0012E8", "#000012"
    };

    if (index < 0 || index >= BW_THEME_COUNT) {
        return "#000000";
    }
    if (style == OS_INSTALLER_STYLE_MODERN) {
        return modern[index];
    }
    if (style == OS_INSTALLER_STYLE_LEGACY) {
        return legacy[index];
    }
    return classic[index];
}

void bw_reset_theme_colors(void)
{
    int style;
    int i;

    style = (int)SendMessageA(g_bw.installer_style, CB_GETCURSEL, 0, 0);
    if (style < OS_INSTALLER_STYLE_CLASSIC || style > OS_INSTALLER_STYLE_LEGACY) {
        style = g_bw.config.installer_style;
    }
    if (style < OS_INSTALLER_STYLE_CLASSIC || style > OS_INSTALLER_STYLE_LEGACY) {
        style = OS_INSTALLER_STYLE_CLASSIC;
    }

    for (i = 0; i < BW_THEME_COUNT; ++i) {
        const char *value = bw_default_theme_value(style, i);
        char *slot = bw_theme_slot_by_index(i);

        if (slot != NULL) {
            bw_copy_component_text(slot, OS_COLOR_TEXT_LEN, value);
        }
        if (g_bw.theme_value[i] != NULL) {
            SetWindowTextA(g_bw.theme_value[i], value);
        }
    }

    bw_save_visible_values();
    InvalidateRect(g_bw.window, NULL, TRUE);
}

static int bw_color_hex_digit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static COLORREF bw_color_from_hex(const char *value, COLORREF fallback)
{
    int r1;
    int r2;
    int g1;
    int g2;
    int b1;
    int b2;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    if (value[0] == '#') {
        ++value;
    }
    if (strlen(value) != 6) {
        return fallback;
    }
    r1 = bw_color_hex_digit(value[0]);
    r2 = bw_color_hex_digit(value[1]);
    g1 = bw_color_hex_digit(value[2]);
    g2 = bw_color_hex_digit(value[3]);
    b1 = bw_color_hex_digit(value[4]);
    b2 = bw_color_hex_digit(value[5]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
        return fallback;
    }
    return RGB((r1 << 4) | r2, (g1 << 4) | g2, (b1 << 4) | b2);
}

static void bw_color_to_hex(COLORREF color, char *out, size_t out_size)
{
    snprintf(out,
             out_size,
             "#%02X%02X%02X",
             (unsigned)GetRValue(color),
             (unsigned)GetGValue(color),
             (unsigned)GetBValue(color));
    out[out_size - 1] = '\0';
}

void bw_pick_theme_color(int index)
{
    static COLORREF custom_colors[16];
    CHOOSECOLORA chooser;
    char value[OS_COLOR_TEXT_LEN];
    COLORREF color;

    if (index < 0 || index >= BW_THEME_COUNT || g_bw.theme_value[index] == NULL) {
        return;
    }

    bw_get(g_bw.theme_value[index], value, sizeof(value));
    color = bw_color_from_hex(value, RGB(0, 120, 212));
    memset(&chooser, 0, sizeof(chooser));
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = g_bw.window;
    chooser.rgbResult = color;
    chooser.lpCustColors = custom_colors;
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorA(&chooser)) {
        bw_color_to_hex(chooser.rgbResult, value, sizeof(value));
        SetWindowTextA(g_bw.theme_value[index], value);
        bw_save_visible_values();
        InvalidateRect(g_bw.window, NULL, TRUE);
    }
}

static void bw_parse_theme_editor(void)
{
    int i;

    for (i = 0; i < BW_THEME_COUNT; ++i) {
        char value[OS_COLOR_TEXT_LEN];
        char *slot = bw_theme_slot_by_index(i);

        if (slot != NULL) {
            bw_get(g_bw.theme_value[i], value, sizeof(value));
            bw_trim(value);
            if (value[0] != '\0') {
                bw_copy_component_text(slot, OS_COLOR_TEXT_LEN, value);
            }
        }
    }
}

void bw_load_theme_editor(void)
{
    int i;

    for (i = 0; i < BW_THEME_COUNT; ++i) {
        char *slot = bw_theme_slot_by_index(i);

        if (slot != NULL) {
            SetWindowTextA(g_bw.theme_value[i], slot);
        }
    }
}

static const char *bw_ui_font_from_index(int index)
{
    switch (index) {
    case 1:
        return "Segoe UI";
    case 2:
        return "Tahoma";
    case 3:
        return "Trebuchet MS";
    case 4:
        return "Georgia";
    case 5:
        return "Consolas";
    default:
        return "";
    }
}

static int bw_ui_font_to_index(const char *font_name)
{
    int i;

    for (i = 1; i <= 5; ++i) {
        if (strcmp(font_name, bw_ui_font_from_index(i)) == 0) {
            return i;
        }
    }
    return 0;
}

int bw_ui_font_selection(void)
{
    return bw_ui_font_to_index(g_bw.config.ui_font);
}

static void bw_text_slots(int index,
                          char **title,
                          size_t *title_size,
                          char **body,
                          size_t *body_size)
{
    switch (index) {
    case 0:
        *title = g_bw.config.welcome_title;
        *title_size = sizeof(g_bw.config.welcome_title);
        *body = g_bw.config.welcome_text;
        *body_size = sizeof(g_bw.config.welcome_text);
        return;
    case 1:
        *title = g_bw.config.folder_title;
        *title_size = sizeof(g_bw.config.folder_title);
        *body = g_bw.config.folder_text;
        *body_size = sizeof(g_bw.config.folder_text);
        return;
    case 2:
        *title = g_bw.config.components_title;
        *title_size = sizeof(g_bw.config.components_title);
        *body = g_bw.config.components_text;
        *body_size = sizeof(g_bw.config.components_text);
        return;
    case 3:
        *title = g_bw.config.ready_title;
        *title_size = sizeof(g_bw.config.ready_title);
        *body = g_bw.config.ready_text;
        *body_size = sizeof(g_bw.config.ready_text);
        return;
    case 4:
        *title = g_bw.config.finish_title;
        *title_size = sizeof(g_bw.config.finish_title);
        *body = g_bw.config.finish_text;
        *body_size = sizeof(g_bw.config.finish_text);
        return;
    default:
        *title = g_bw.config.uninstall_title;
        *title_size = sizeof(g_bw.config.uninstall_title);
        *body = g_bw.config.uninstall_text;
        *body_size = sizeof(g_bw.config.uninstall_text);
        return;
    }
}

void bw_load_text_editor(void)
{
    char *title;
    char *body;
    size_t title_size;
    size_t body_size;

    if (g_bw.text_tabs != NULL) {
        TabCtrl_SetCurSel(g_bw.text_tabs, g_bw.selected_text_page);
    }
    bw_text_slots(g_bw.selected_text_page, &title, &title_size, &body, &body_size);
    (void)title_size;
    (void)body_size;
    SetWindowTextA(g_bw.page_title, title);
    SetWindowTextA(g_bw.page_body, body);
}

void bw_save_text_editor(void)
{
    char *title;
    char *body;
    size_t title_size;
    size_t body_size;

    bw_text_slots(g_bw.selected_text_page, &title, &title_size, &body, &body_size);
    bw_get(g_bw.page_title, title, title_size);
    bw_get(g_bw.page_body, body, body_size);
}

void bw_save_visible_values(void)
{
    bw_get(g_bw.app_name, g_bw.config.app_name, sizeof(g_bw.config.app_name));
    bw_get(g_bw.company_name, g_bw.config.company_name, sizeof(g_bw.config.company_name));
    bw_get(g_bw.app_version, g_bw.config.app_version, sizeof(g_bw.config.app_version));
    bw_get(g_bw.install_dir, g_bw.config.install_dir, sizeof(g_bw.config.install_dir));
    bw_get(g_bw.source_dir, g_bw.config.source_dir, sizeof(g_bw.config.source_dir));
    bw_get(g_bw.output_dir, g_bw.config.output_dir, sizeof(g_bw.config.output_dir));
    bw_get(g_bw.license_file, g_bw.config.license_file, sizeof(g_bw.config.license_file));
    bw_get(g_bw.wizard_image, g_bw.config.wizard_image_file, sizeof(g_bw.config.wizard_image_file));
    bw_get(g_bw.background_image, g_bw.config.background_image_file, sizeof(g_bw.config.background_image_file));
    bw_get(g_bw.installer_icon, g_bw.config.installer_icon_file, sizeof(g_bw.config.installer_icon_file));
    bw_get(g_bw.uninstaller_icon, g_bw.config.uninstaller_icon_file, sizeof(g_bw.config.uninstaller_icon_file));
    bw_get(g_bw.launcher, g_bw.config.launcher, sizeof(g_bw.config.launcher));
    g_bw.config.include_license = g_bw.config.license_file[0] != '\0';
    g_bw.config.generate_native_exe = SendMessageA(g_bw.native_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_bw.config.register_system = SendMessageA(g_bw.register_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_bw.config.generate_windows = SendMessageA(g_bw.windows_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_bw.config.generate_unix = SendMessageA(g_bw.unix_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_bw.config.installer_style = (int)SendMessageA(g_bw.installer_style, CB_GETCURSEL, 0, 0);
    if (g_bw.config.installer_style < OS_INSTALLER_STYLE_CLASSIC ||
        g_bw.config.installer_style > OS_INSTALLER_STYLE_LEGACY) {
        g_bw.config.installer_style = OS_INSTALLER_STYLE_CLASSIC;
    }
    g_bw.config.window_style = (int)SendMessageA(g_bw.window_style, CB_GETCURSEL, 0, 0);
    if (g_bw.config.window_style < OS_WINDOW_STYLE_FIXED || g_bw.config.window_style > OS_WINDOW_STYLE_MAXIMIZED) {
        g_bw.config.window_style = OS_WINDOW_STYLE_FIXED;
    }
    strncpy(g_bw.config.ui_font,
            bw_ui_font_from_index((int)SendMessageA(g_bw.ui_font, CB_GETCURSEL, 0, 0)),
            sizeof(g_bw.config.ui_font) - 1);
    g_bw.config.ui_font[sizeof(g_bw.config.ui_font) - 1] = '\0';
    bw_parse_page_flow_editor();
    bw_parse_theme_editor();
    bw_parse_online_editor();
    if (g_bw.page == BW_PAGE_TEXTS) {
        bw_save_text_editor();
    }
    bw_set_runtime_exe(g_bw.config.runtime_exe, sizeof(g_bw.config.runtime_exe));
}

static void bw_hide_all_page_controls(void)
{
    HWND controls[] = {
        g_bw.app_name_label, g_bw.company_name_label, g_bw.app_version_label, g_bw.install_dir_label,
        g_bw.app_name, g_bw.company_name, g_bw.app_version, g_bw.install_dir,
        g_bw.source_dir_label, g_bw.output_dir_label, g_bw.license_file_label, g_bw.wizard_image_label,
        g_bw.background_image_label,
        g_bw.launcher_label, g_bw.installer_icon_label, g_bw.uninstaller_icon_label,
        g_bw.source_dir, g_bw.output_dir, g_bw.license_file, g_bw.wizard_image,
        g_bw.background_image, g_bw.launcher, g_bw.installer_icon, g_bw.uninstaller_icon,
        g_bw.text_page_label, g_bw.page_title_label, g_bw.page_body_label,
        g_bw.text_page, g_bw.text_tabs, g_bw.page_title, g_bw.page_body,
        g_bw.installer_style_label, g_bw.installer_style,
        g_bw.ui_font_label, g_bw.ui_font, g_bw.window_style_label, g_bw.window_style,
        g_bw.page_flow_label, g_bw.page_flow, g_bw.page_flow_count,
        g_bw.theme_colors_label, g_bw.theme_colors, g_bw.theme_reset,
        g_bw.online_components_label, g_bw.online_components,
        g_bw.native_box, g_bw.register_box, g_bw.windows_box, g_bw.unix_box,
        g_bw.status, g_bw.open_output,
        GetDlgItem(g_bw.window, BW_ID_SOURCE_BROWSE),
        GetDlgItem(g_bw.window, BW_ID_OUTPUT_BROWSE),
        GetDlgItem(g_bw.window, BW_ID_LICENSE_BROWSE),
        GetDlgItem(g_bw.window, BW_ID_IMAGE_BROWSE),
        GetDlgItem(g_bw.window, BW_ID_BACKGROUND_IMAGE_BROWSE),
        GetDlgItem(g_bw.window, BW_ID_INSTALLER_ICON_BROWSE),
        GetDlgItem(g_bw.window, BW_ID_UNINSTALLER_ICON_BROWSE)
    };
    int i;

    for (i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); ++i) {
        bw_show(controls[i], 0);
    }
    for (i = 0; i < BW_THEME_COUNT; ++i) {
        bw_show(g_bw.theme_name[i], 0);
        bw_show(g_bw.theme_value[i], 0);
        bw_show(g_bw.theme_pick[i], 0);
    }
    for (i = 0; i < 6; ++i) {
        bw_show(g_bw.page_flow_checks[i], 0);
    }
    for (i = 0; i < 5; ++i) {
        bw_show(g_bw.online_header[i], 0);
    }
    for (i = 0; i < BW_ONLINE_ROWS; ++i) {
        bw_show(g_bw.online_name[i], 0);
        bw_show(g_bw.online_url[i], 0);
        bw_show(g_bw.online_target[i], 0);
        bw_show(g_bw.online_page[i], 0);
        bw_show(g_bw.online_default[i], 0);
    }
}

void bw_set_page(BwPage page)
{
    g_bw.page = page;
    bw_hide_all_page_controls();
    EnableWindow(g_bw.back, page != BW_PAGE_APP && page != BW_PAGE_DONE);
    SetWindowTextA(g_bw.next,
                   page == BW_PAGE_BUILD ? "Build" :
                   page == BW_PAGE_DONE ? os_win32_text(OS_WIN32_TEXT_FINISH) :
                   os_win32_text(OS_WIN32_TEXT_NEXT));

    if (page == BW_PAGE_APP) {
        SetWindowTextA(g_bw.title, "Application");
        SetWindowTextA(g_bw.subtitle, "Name the program, publisher, version, and default installation folder.");
        bw_show(g_bw.app_name_label, 1);
        bw_show(g_bw.company_name_label, 1);
        bw_show(g_bw.app_version_label, 1);
        bw_show(g_bw.install_dir_label, 1);
        bw_show(g_bw.app_name, 1);
        bw_show(g_bw.company_name, 1);
        bw_show(g_bw.app_version, 1);
        bw_show(g_bw.install_dir, 1);
    } else if (page == BW_PAGE_FILES) {
        SetWindowTextA(g_bw.title, "Files");
        SetWindowTextA(g_bw.subtitle, "Choose the program folder, output folder, license, wizard artwork, and EXE icons.");
        bw_show(g_bw.source_dir_label, 1);
        bw_show(g_bw.output_dir_label, 1);
        bw_show(g_bw.license_file_label, 1);
        bw_show(g_bw.wizard_image_label, 1);
        bw_show(g_bw.background_image_label, 1);
        bw_show(g_bw.launcher_label, 1);
        bw_show(g_bw.installer_icon_label, 1);
        bw_show(g_bw.uninstaller_icon_label, 1);
        bw_show(g_bw.source_dir, 1);
        bw_show(g_bw.output_dir, 1);
        bw_show(g_bw.license_file, 1);
        bw_show(g_bw.wizard_image, 1);
        bw_show(g_bw.background_image, 1);
        bw_show(g_bw.launcher, 1);
        bw_show(g_bw.installer_icon, 1);
        bw_show(g_bw.uninstaller_icon, 1);
        bw_show(GetDlgItem(g_bw.window, BW_ID_SOURCE_BROWSE), 1);
        bw_show(GetDlgItem(g_bw.window, BW_ID_OUTPUT_BROWSE), 1);
        bw_show(GetDlgItem(g_bw.window, BW_ID_LICENSE_BROWSE), 1);
        bw_show(GetDlgItem(g_bw.window, BW_ID_IMAGE_BROWSE), 1);
        bw_show(GetDlgItem(g_bw.window, BW_ID_BACKGROUND_IMAGE_BROWSE), 1);
        bw_show(GetDlgItem(g_bw.window, BW_ID_INSTALLER_ICON_BROWSE), 1);
        bw_show(GetDlgItem(g_bw.window, BW_ID_UNINSTALLER_ICON_BROWSE), 1);
    } else if (page == BW_PAGE_TEXTS) {
        SetWindowTextA(g_bw.title, "Wizard Pages");
        SetWindowTextA(g_bw.subtitle, "Write the text users will see inside the installer and uninstaller.");
        bw_show(g_bw.text_page_label, 1);
        bw_show(g_bw.page_title_label, 1);
        bw_show(g_bw.page_body_label, 1);
        bw_show(g_bw.text_tabs, 1);
        bw_show(g_bw.page_title, 1);
        bw_show(g_bw.page_body, 1);
        bw_load_text_editor();
    } else if (page == BW_PAGE_OPTIONS) {
        SetWindowTextA(g_bw.title, "Options");
        SetWindowTextA(g_bw.subtitle, "Choose a base template, page flow, colors, optional downloads, and package targets.");
        bw_load_page_flow_editor();
        bw_load_theme_editor();
        bw_load_online_editor();
        bw_show(g_bw.installer_style_label, 1);
        bw_show(g_bw.installer_style, 1);
        bw_show(g_bw.ui_font_label, 1);
        bw_show(g_bw.ui_font, 1);
        bw_show(g_bw.window_style_label, 1);
        bw_show(g_bw.window_style, 1);
        bw_show(g_bw.page_flow_label, 1);
        bw_show(g_bw.page_flow_count, 1);
        {
            int i;
            for (i = 0; i < 6; ++i) {
                bw_show(g_bw.page_flow_checks[i], 1);
            }
        }
        bw_show(g_bw.theme_colors_label, 1);
        bw_show(g_bw.theme_reset, 1);
        {
            int i;
            for (i = 0; i < BW_THEME_COUNT; ++i) {
                bw_show(g_bw.theme_name[i], 1);
                bw_show(g_bw.theme_value[i], 1);
                bw_show(g_bw.theme_pick[i], 1);
            }
        }
        bw_show(g_bw.online_components_label, 1);
        {
            int i;
            for (i = 0; i < 5; ++i) {
                bw_show(g_bw.online_header[i], 1);
            }
            for (i = 0; i < BW_ONLINE_ROWS; ++i) {
                bw_show(g_bw.online_name[i], 1);
                bw_show(g_bw.online_url[i], 1);
                bw_show(g_bw.online_target[i], 1);
                bw_show(g_bw.online_page[i], 1);
                bw_show(g_bw.online_default[i], 1);
            }
        }
        bw_show(g_bw.native_box, 1);
        bw_show(g_bw.register_box, 1);
        bw_show(g_bw.windows_box, 1);
        bw_show(g_bw.unix_box, 1);
    } else if (page == BW_PAGE_BUILD) {
        char summary[OS_MAX_MESSAGE_LEN + OS_MAX_PATH_LEN];
        SetWindowTextA(g_bw.title, "Ready to Build");
        SetWindowTextA(g_bw.subtitle, "Review the package and click Build.");
        snprintf(summary,
                 sizeof(summary),
                 "Program: %s\r\nCompany: %s\r\nVersion: %s\r\n\r\nInstaller will be created inside:\r\n%s\r\n\r\nSource folder:\r\n%s",
                 g_bw.config.app_name,
                 g_bw.config.company_name[0] != '\0' ? g_bw.config.company_name : "(not set)",
                 g_bw.config.app_version,
                 g_bw.config.output_dir,
                 g_bw.config.source_dir);
        SetWindowTextA(g_bw.status, summary);
        bw_show(g_bw.status, 1);
    } else {
        char done[OS_MAX_MESSAGE_LEN + OS_MAX_PATH_LEN * 2];
        SetWindowTextA(g_bw.title, "Finished");
        SetWindowTextA(g_bw.subtitle, g_bw.generated ? "The installer package was created." : "The package could not be created.");
        if (g_bw.generated) {
            snprintf(done,
                     sizeof(done),
                     "Created package:\r\n%s\r\n\r\nMain file:\r\n%s\\installer.exe\r\n\r\nThe uninstaller is embedded in installer.exe and will be written into the installed application folder.",
                     g_bw.result.package_dir,
                     g_bw.result.package_dir);
            SetWindowTextA(g_bw.status, done);
            bw_show(g_bw.open_output, 1);
        } else {
            SetWindowTextA(g_bw.status, g_bw.result.message);
        }
        bw_show(g_bw.status, 1);
        EnableWindow(g_bw.back, FALSE);
    }

    InvalidateRect(g_bw.window, NULL, TRUE);
}

int bw_step_from_point(int x, int y)
{
    int i;

    if (x < 0 || x >= BW_RAIL_W) {
        return -1;
    }

    for (i = 0; i <= (int)BW_PAGE_DONE; ++i) {
        int center = BW_STEP_START_Y + i * BW_STEP_GAP_Y;
        if (y >= center - 18 && y <= center + 24) {
            return i;
        }
    }

    return -1;
}

void bw_go_to_page(BwPage page)
{
    if (page < BW_PAGE_APP || page > BW_PAGE_DONE) {
        return;
    }

    if (page == BW_PAGE_DONE && !g_bw.generated) {
        return;
    }

    if (page == g_bw.page) {
        return;
    }

    bw_save_visible_values();
    bw_set_page(page);
}

void bw_next(void)
{
    if (g_bw.page == BW_PAGE_DONE) {
        DestroyWindow(g_bw.window);
        return;
    }

    bw_save_visible_values();
    if (g_bw.page == BW_PAGE_BUILD) {
        g_bw.generated = os_generate_project(&g_bw.config, &g_bw.result) == 0;
        bw_set_page(BW_PAGE_DONE);
        return;
    }

    bw_set_page((BwPage)(g_bw.page + 1));
}

void bw_back(void)
{
    if (g_bw.page > BW_PAGE_APP && g_bw.page < BW_PAGE_DONE) {
        bw_save_visible_values();
        bw_set_page((BwPage)(g_bw.page - 1));
    }
}

void bw_pick_source(void)
{
    BROWSEINFOA browse;
    PIDLIST_ABSOLUTE item;
    char path[OS_MAX_PATH_LEN];

    memset(&browse, 0, sizeof(browse));
    browse.hwndOwner = g_bw.window;
    browse.lpszTitle = "Select program files folder";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    item = SHBrowseForFolderA(&browse);
    if (item != NULL) {
        if (SHGetPathFromIDListA(item, path)) {
            SetWindowTextA(g_bw.source_dir, path);
        }
        CoTaskMemFree(item);
    }
}

void bw_pick_output(void)
{
    BROWSEINFOA browse;
    PIDLIST_ABSOLUTE item;
    char path[OS_MAX_PATH_LEN];

    memset(&browse, 0, sizeof(browse));
    browse.hwndOwner = g_bw.window;
    browse.lpszTitle = "Select output folder";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    item = SHBrowseForFolderA(&browse);
    if (item != NULL) {
        if (SHGetPathFromIDListA(item, path)) {
            SetWindowTextA(g_bw.output_dir, path);
        }
        CoTaskMemFree(item);
    }
}

void bw_pick_license(void)
{
    OPENFILENAMEA ofn;
    char path[OS_MAX_PATH_LEN];

    memset(path, 0, sizeof(path));
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_bw.window;
    ofn.lpstrFilter = "Text and license files\0*.txt;*.md;LICENSE;LICENSE.*\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        SetWindowTextA(g_bw.license_file, path);
    }
}

void bw_pick_wizard_image(void)
{
    OPENFILENAMEA ofn;
    char path[OS_MAX_PATH_LEN];
    static const char image_filter[] =
        "Installer images\0*.bmp;*.png\0"
        "Bitmap images\0*.bmp\0"
        "PNG images\0*.png\0"
        "All files\0*.*\0";

    memset(path, 0, sizeof(path));
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_bw.window;
    ofn.lpstrFilter = image_filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        SetWindowTextA(g_bw.wizard_image, path);
    }
}

void bw_pick_background_image(void)
{
    OPENFILENAMEA ofn;
    char path[OS_MAX_PATH_LEN];
    static const char image_filter[] =
        "Installer images\0*.bmp;*.png\0"
        "Bitmap images\0*.bmp\0"
        "PNG images\0*.png\0"
        "All files\0*.*\0";

    memset(path, 0, sizeof(path));
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_bw.window;
    ofn.lpstrFilter = image_filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        SetWindowTextA(g_bw.background_image, path);
    }
}

static void bw_pick_icon_into(HWND target)
{
    OPENFILENAMEA ofn;
    char path[OS_MAX_PATH_LEN];

    memset(path, 0, sizeof(path));
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_bw.window;
    ofn.lpstrFilter = "Windows icon files\0*.ico\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        SetWindowTextA(target, path);
    }
}

void bw_pick_installer_icon(void)
{
    bw_pick_icon_into(g_bw.installer_icon);
}

void bw_pick_uninstaller_icon(void)
{
    bw_pick_icon_into(g_bw.uninstaller_icon);
}
